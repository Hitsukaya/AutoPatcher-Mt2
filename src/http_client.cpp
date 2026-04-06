#include "../include/http_client.hpp"
#include "file_ops.hpp"

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>
#include <chrono>
#include <vector>

namespace
{
    constexpr int kMaxDownloadRetries = 5;
    constexpr int kRetryDelaySeconds = 2;
    constexpr int kConnectTimeoutMs = 15000;
    constexpr int kSendTimeoutMs = 15000;
    constexpr int kReceiveTimeoutMs = 60000;

#ifdef _WIN32
    struct ParsedUrl
    {
        bool secure = true;
        INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
        std::wstring host;
        std::wstring path;
    };

    std::wstring utf8_to_wstring(const std::string &text)
    {
        if (text.empty())
        {
            return {};
        }

        const int size_needed = MultiByteToWideChar(
            CP_UTF8,
            0,
            text.c_str(),
            -1,
            nullptr,
            0);

        if (size_needed <= 0)
        {
            throw std::runtime_error("Failed to convert UTF-8 to wide string");
        }

        std::wstring result(static_cast<std::size_t>(size_needed - 1), L'\0');

        MultiByteToWideChar(
            CP_UTF8,
            0,
            text.c_str(),
            -1,
            result.data(),
            size_needed);

        return result;
    }

    std::string wstring_to_utf8(const std::wstring &text)
    {
        if (text.empty())
        {
            return {};
        }

        const int size_needed = WideCharToMultiByte(
            CP_UTF8,
            0,
            text.c_str(),
            -1,
            nullptr,
            0,
            nullptr,
            nullptr);

        if (size_needed <= 0)
        {
            throw std::runtime_error("Failed to convert wide string to UTF-8");
        }

        std::string result(static_cast<std::size_t>(size_needed - 1), '\0');

        WideCharToMultiByte(
            CP_UTF8,
            0,
            text.c_str(),
            -1,
            result.data(),
            size_needed,
            nullptr,
            nullptr);

        return result;
    }

    std::string narrow_ascii(const std::wstring &text)
    {
        return wstring_to_utf8(text);
    }

    ParsedUrl parse_url(const std::string &url)
    {
        const std::wstring wide_url = utf8_to_wstring(url);

        URL_COMPONENTSW uc{};
        uc.dwStructSize = sizeof(uc);

        wchar_t host_name[2048]{};
        wchar_t url_path[8192]{};
        wchar_t extra_info[4096]{};

        uc.lpszHostName = host_name;
        uc.dwHostNameLength = static_cast<DWORD>(std::size(host_name));

        uc.lpszUrlPath = url_path;
        uc.dwUrlPathLength = static_cast<DWORD>(std::size(url_path));

        uc.lpszExtraInfo = extra_info;
        uc.dwExtraInfoLength = static_cast<DWORD>(std::size(extra_info));

        if (!WinHttpCrackUrl(wide_url.c_str(), 0, 0, &uc))
        {
            throw std::runtime_error("Failed to parse URL: " + url);
        }

        ParsedUrl parsed{};
        parsed.secure = (uc.nScheme == INTERNET_SCHEME_HTTPS);
        parsed.port = uc.nPort;
        parsed.host.assign(uc.lpszHostName, uc.dwHostNameLength);
        parsed.path.assign(uc.lpszUrlPath, uc.dwUrlPathLength);

        if (uc.dwExtraInfoLength > 0)
        {
            parsed.path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
        }

        if (parsed.path.empty())
        {
            parsed.path = L"/";
        }

        return parsed;
    }

    struct WinHttpHandle
    {
        HINTERNET handle = nullptr;

        WinHttpHandle() = default;
        explicit WinHttpHandle(HINTERNET h) : handle(h) {}

        ~WinHttpHandle()
        {
            if (handle)
            {
                WinHttpCloseHandle(handle);
                handle = nullptr;
            }
        }

        WinHttpHandle(const WinHttpHandle &) = delete;
        WinHttpHandle &operator=(const WinHttpHandle &) = delete;

        WinHttpHandle(WinHttpHandle &&other) noexcept : handle(other.handle)
        {
            other.handle = nullptr;
        }

        WinHttpHandle &operator=(WinHttpHandle &&other) noexcept
        {
            if (this != &other)
            {
                if (handle)
                {
                    WinHttpCloseHandle(handle);
                }

                handle = other.handle;
                other.handle = nullptr;
            }

            return *this;
        }

        operator HINTERNET() const
        {
            return handle;
        }

        bool valid() const
        {
            return handle != nullptr;
        }
    };

    std::string get_last_error_message(const std::string &prefix)
    {
        const DWORD error = GetLastError();
        return prefix + " | Win32 error: " + std::to_string(error);
    }

    void set_timeouts(HINTERNET handle)
    {
        WinHttpSetTimeouts(
            handle,
            kConnectTimeoutMs,
            kConnectTimeoutMs,
            kSendTimeoutMs,
            kReceiveTimeoutMs);
    }

    WinHttpHandle open_session()
    {
        WinHttpHandle session(WinHttpOpen(
            L"Hitsukaya Nyx2 Launcher/1.0",
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0));

        if (!session.valid())
        {
            throw std::runtime_error(get_last_error_message("WinHttpOpen failed"));
        }

        set_timeouts(session);
        return session;
    }

    WinHttpHandle open_connection(HINTERNET session, const ParsedUrl &url)
    {
        WinHttpHandle connection(WinHttpConnect(
            session,
            url.host.c_str(),
            url.port,
            0));

        if (!connection.valid())
        {
            throw std::runtime_error(
                get_last_error_message("WinHttpConnect failed for host: " + narrow_ascii(url.host)));
        }

        return connection;
    }

    WinHttpHandle open_request(HINTERNET connection, const ParsedUrl &url, const wchar_t *verb)
    {
        const DWORD flags = url.secure ? WINHTTP_FLAG_SECURE : 0;

        WinHttpHandle request(WinHttpOpenRequest(
            connection,
            verb,
            url.path.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            flags));

        if (!request.valid())
        {
            throw std::runtime_error(
                get_last_error_message("WinHttpOpenRequest failed for URL path: " + narrow_ascii(url.path)));
        }

        set_timeouts(request);

        return request;
    }

    void send_request(HINTERNET request)
    {
        if (!WinHttpSendRequest(
                request,
                WINHTTP_NO_ADDITIONAL_HEADERS,
                0,
                WINHTTP_NO_REQUEST_DATA,
                0,
                0,
                0))
        {
            throw std::runtime_error(get_last_error_message("WinHttpSendRequest failed"));
        }

        if (!WinHttpReceiveResponse(request, nullptr))
        {
            throw std::runtime_error(get_last_error_message("WinHttpReceiveResponse failed"));
        }
    }

    DWORD query_status_code(HINTERNET request)
    {
        DWORD status_code = 0;
        DWORD size = sizeof(status_code);

        if (!WinHttpQueryHeaders(
                request,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &status_code,
                &size,
                WINHTTP_NO_HEADER_INDEX))
        {
            throw std::runtime_error(get_last_error_message("Failed to query HTTP status code"));
        }

        return status_code;
    }

    std::uint64_t query_content_length(HINTERNET request)
    {
        wchar_t buffer[64]{};
        DWORD size = sizeof(buffer);

        if (!WinHttpQueryHeaders(
                request,
                WINHTTP_QUERY_CONTENT_LENGTH,
                WINHTTP_HEADER_NAME_BY_INDEX,
                buffer,
                &size,
                WINHTTP_NO_HEADER_INDEX))
        {
            return 0;
        }

        try
        {
            return static_cast<std::uint64_t>(std::stoull(buffer));
        }
        catch (...)
        {
            return 0;
        }
    }

    std::string read_response_to_string(HINTERNET request)
    {
        std::string response;
        std::vector<char> buffer(16 * 1024);

        while (true)
        {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(request, &available))
            {
                throw std::runtime_error(get_last_error_message("WinHttpQueryDataAvailable failed"));
            }

            if (available == 0)
            {
                break;
            }

            if (available > buffer.size())
            {
                buffer.resize(available);
            }

            DWORD downloaded = 0;
            if (!WinHttpReadData(request, buffer.data(), available, &downloaded))
            {
                throw std::runtime_error(get_last_error_message("WinHttpReadData failed"));
            }

            response.append(buffer.data(), buffer.data() + downloaded);
        }

        return response;
    }

    bool should_retry_http_status(DWORD status)
    {
        return status == 408 || status == 425 || status == 429 ||
               status == 500 || status == 502 || status == 503 || status == 504;
    }

    bool should_retry_win32_error(DWORD err)
    {
        switch (err)
        {
        case ERROR_WINHTTP_TIMEOUT:
        case ERROR_WINHTTP_CANNOT_CONNECT:
        case ERROR_WINHTTP_CONNECTION_ERROR:
        case ERROR_WINHTTP_NAME_NOT_RESOLVED:
        case ERROR_WINHTTP_SECURE_FAILURE:
        case ERROR_WINHTTP_RESEND_REQUEST:
            return true;
        default:
            return false;
        }
    }
#endif
}

HttpClient::HttpClient() = default;
HttpClient::~HttpClient() = default;

std::string HttpClient::get_text(const std::string &url) const
{
#ifndef _WIN32
    throw std::runtime_error("HttpClient WinHTTP implementation is only available on Windows");
#else
    std::string last_error;

    for (int attempt = 1; attempt <= kMaxDownloadRetries; ++attempt)
    {
        try
        {
            const ParsedUrl parsed = parse_url(url);

            WinHttpHandle session = open_session();
            WinHttpHandle connection = open_connection(session, parsed);
            WinHttpHandle request = open_request(connection, parsed, L"GET");

            send_request(request);

            const DWORD status = query_status_code(request);
            if (status < 200 || status >= 300)
            {
                if (should_retry_http_status(status) && attempt < kMaxDownloadRetries)
                {
                    last_error = "HTTP GET returned status code: " + std::to_string(status) + " | URL: " + url;
                    std::this_thread::sleep_for(std::chrono::seconds(kRetryDelaySeconds));
                    continue;
                }

                throw std::runtime_error(
                    "HTTP GET returned status code: " + std::to_string(status) + " | URL: " + url);
            }

            return read_response_to_string(request);
        }
        catch (const std::exception &ex)
        {
            last_error = ex.what();

            const DWORD err = GetLastError();
            if (attempt < kMaxDownloadRetries && should_retry_win32_error(err))
            {
                std::this_thread::sleep_for(std::chrono::seconds(kRetryDelaySeconds));
                continue;
            }

            if (attempt == kMaxDownloadRetries)
            {
                throw;
            }
        }
    }

    throw std::runtime_error("HTTP GET failed after retries: " + last_error);
#endif
}

void HttpClient::download_to_file(
    const std::string &url,
    const std::string &output_path,
    std::uint64_t expected_size,
    ProgressCallback progress_callback_fn) const
{
#ifndef _WIN32
    throw std::runtime_error("HttpClient WinHTTP implementation is only available on Windows");
#else
    file_ops::ensure_parent_directory(output_path);

    const std::string temp_path = output_path + ".download";
    std::string last_error;

    for (int attempt = 1; attempt <= kMaxDownloadRetries; ++attempt)
    {
        try
        {
            if (file_ops::exists(temp_path))
            {
                file_ops::remove_file(temp_path);
            }

            const ParsedUrl parsed = parse_url(url);

            WinHttpHandle session = open_session();
            WinHttpHandle connection = open_connection(session, parsed);
            WinHttpHandle request = open_request(connection, parsed, L"GET");

            send_request(request);

            const DWORD status = query_status_code(request);
            if (status < 200 || status >= 300)
            {
                if (should_retry_http_status(status) && attempt < kMaxDownloadRetries)
                {
                    last_error = "File download returned status code: " + std::to_string(status) + " | URL: " + url;
                    std::this_thread::sleep_for(std::chrono::seconds(kRetryDelaySeconds));
                    continue;
                }

                throw std::runtime_error(
                    "File download returned status code: " + std::to_string(status) + " | URL: " + url);
            }

            std::uint64_t total_size = expected_size;
            if (total_size == 0)
            {
                total_size = query_content_length(request);
            }

            FILE *file = std::fopen(temp_path.c_str(), "wb");
            if (!file)
            {
                throw std::runtime_error("Failed to open output file: " + temp_path);
            }

            std::uint64_t downloaded_total = 0;
            std::vector<char> buffer(64 * 1024);

            while (true)
            {
                DWORD available = 0;
                if (!WinHttpQueryDataAvailable(request, &available))
                {
                    std::fclose(file);
                    throw std::runtime_error(get_last_error_message("WinHttpQueryDataAvailable failed"));
                }

                if (available == 0)
                {
                    break;
                }

                if (available > buffer.size())
                {
                    buffer.resize(available);
                }

                DWORD downloaded = 0;
                if (!WinHttpReadData(request, buffer.data(), available, &downloaded))
                {
                    std::fclose(file);
                    throw std::runtime_error(get_last_error_message("WinHttpReadData failed"));
                }

                const std::size_t written = std::fwrite(buffer.data(), 1, downloaded, file);
                if (written != downloaded)
                {
                    std::fclose(file);
                    throw std::runtime_error("Failed to write downloaded data to file: " + temp_path);
                }

                downloaded_total += static_cast<std::uint64_t>(downloaded);

                if (progress_callback_fn)
                {
                    progress_callback_fn(
                        static_cast<double>(total_size),
                        static_cast<double>(downloaded_total));
                }
            }

            std::fclose(file);

            const std::uint64_t final_size = file_ops::get_file_size(temp_path);
            if (expected_size > 0 && final_size != expected_size)
            {
                throw std::runtime_error(
                    "Downloaded file size mismatch. Expected: " +
                    std::to_string(expected_size) +
                    ", got: " + std::to_string(final_size) +
                    " | URL: " + url);
            }

            if (file_ops::exists(output_path))
            {
                file_ops::remove_file(output_path);
            }

            if (std::rename(temp_path.c_str(), output_path.c_str()) != 0)
            {
                throw std::runtime_error(
                    "Failed to rename temp file to final path: " + output_path);
            }

            if (progress_callback_fn)
            {
                const double total =
                    expected_size > 0
                        ? static_cast<double>(expected_size)
                        : static_cast<double>(file_ops::get_file_size(output_path));

                progress_callback_fn(total, total);
            }

            return;
        }
        catch (const std::exception &ex)
        {
            last_error = ex.what();

            if (file_ops::exists(temp_path))
            {
                file_ops::remove_file(temp_path);
            }

            const DWORD err = GetLastError();
            if (attempt < kMaxDownloadRetries && should_retry_win32_error(err))
            {
                std::this_thread::sleep_for(std::chrono::seconds(kRetryDelaySeconds));
                continue;
            }

            if (attempt == kMaxDownloadRetries)
            {
                throw;
            }

            std::this_thread::sleep_for(std::chrono::seconds(kRetryDelaySeconds));
        }
    }

    throw std::runtime_error("File download failed after retries: " + last_error);
#endif
}