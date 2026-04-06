#pragma once

#include <cstdint>
#include <functional>
#include <string>

class HttpClient
{
public:
    using ProgressCallback = std::function<void(double total_bytes, double downloaded_bytes)>;

    HttpClient();
    ~HttpClient();

    HttpClient(const HttpClient &) = delete;
    HttpClient &operator=(const HttpClient &) = delete;

    HttpClient(HttpClient &&) = delete;
    HttpClient &operator=(HttpClient &&) = delete;

    std::string get_text(const std::string &url) const;

    void download_to_file(
        const std::string &url,
        const std::string &output_path,
        std::uint64_t expected_size,
        ProgressCallback progress_callback = nullptr) const;
};