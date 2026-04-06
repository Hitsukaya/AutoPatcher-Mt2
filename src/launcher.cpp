#include "../include/launcher.hpp"

#include "../include/patcher.hpp"
#include "../include/ui.hpp"
#include "../include/version.hpp"

#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace
{
    constexpr const char *kLocalVersionFile = "version.txt";
    constexpr const char *kClientRoot = "Client";
    constexpr const char *kRemoteVersionUrl =
        "https://eu2.contabostorage.com/daabf82772a64933a29f62dacfdb6091:hitsukaya-doragon/Nyx2Patcher/version.json";

    std::wstring to_wstring_simple(const std::string &text)
    {
        return std::wstring(text.begin(), text.end());
    }

    std::string to_string_utf8(const std::wstring &text)
    {
#ifdef _WIN32
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
#else
        return std::string(text.begin(), text.end());
#endif
    }

    bool path_exists(const std::filesystem::path &path)
    {
        std::error_code ec;
        return std::filesystem::exists(path, ec);
    }

    void ensure_directory_exists(const std::filesystem::path &path)
    {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec))
        {
            if (!std::filesystem::create_directories(path, ec) && ec)
            {
                throw std::runtime_error("Failed to create directory: " + path.string());
            }
        }
    }

    void set_error_state(const std::string &message)
    {
        ui::set_status_text(L"Patch failed");
        ui::set_progress_text(to_wstring_simple(message));
        ui::set_controls_enabled(true);
        ui::set_start_enabled(true);
    }

    enum class ClientState
    {
        FreshInstall,
        MissingExecutable,
        ReadyForPatch
    };

    ClientState detect_client_state()
    {
        const std::filesystem::path client_dir = kClientRoot;
        const std::wstring game_path_w = ui::game_path();
        const std::filesystem::path game_path(game_path_w);

        if (!path_exists(client_dir))
        {
            return ClientState::FreshInstall;
        }

        if (!path_exists(game_path))
        {
            return ClientState::MissingExecutable;
        }

        return ClientState::ReadyForPatch;
    }

    void launch_game_checked()
    {
        const std::wstring game_path_w = ui::game_path();
        const std::string game_path = to_string_utf8(game_path_w);

        if (game_path.empty())
        {
            throw std::runtime_error("Game path is empty in launcher configuration");
        }

        if (!path_exists(std::filesystem::path(game_path_w)))
        {
            throw std::runtime_error("Game executable not found: " + game_path);
        }

#ifdef _WIN32
        const std::filesystem::path exe_fs(game_path_w);
        const std::wstring working_dir = exe_fs.parent_path().wstring();

        HINSTANCE result = ShellExecuteW(
            nullptr,
            L"open",
            game_path_w.c_str(),
            nullptr,
            working_dir.c_str(),
            SW_SHOWNORMAL);

        if ((INT_PTR)result <= 32)
        {
            throw std::runtime_error(
                "Failed to launch game: " + game_path +
                " | ShellExecute=" + std::to_string((INT_PTR)result));
        }
#else
        throw std::runtime_error("launch_game is only implemented on Windows");
#endif
    }

    void finalize_and_launch()
    {
        ui::set_status_text(L"Client ready. Launching game...");
        ui::set_progress_text(L"Launch ready");
        ui::set_progress_value(100);

        launch_game_checked();

        HWND hwnd = ui::main_window();
        if (!hwnd)
        {
            throw std::runtime_error("Main window handle is null");
        }

        ShowWindow(hwnd, SW_HIDE);
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
    }

    void run_patch_flow(bool repair_mode)
    {
        std::thread([repair_mode]()
                    {
            try
            {
                ui::set_controls_enabled(false);
                ui::set_start_enabled(false);
                ui::set_progress_value(0);

                const ClientState state = detect_client_state();

                switch (state)
                {
                case ClientState::FreshInstall:
                    ui::set_status_text(L"Client missing. Starting full install...");
                    ui::set_progress_text(L"Preparing client folder...");
                    ensure_directory_exists(std::filesystem::path(kClientRoot));
                    break;

                case ClientState::MissingExecutable:
                    ui::set_status_text(L"Client incomplete. Repairing installation...");
                    ui::set_progress_text(L"Preparing repair flow...");
                    ensure_directory_exists(std::filesystem::path(kClientRoot));
                    break;

                case ClientState::ReadyForPatch:
                    if (repair_mode)
                    {
                        ui::set_status_text(L"Repairing client...");
                        ui::set_progress_text(L"Checking local files...");
                    }
                    else
                    {
                        ui::set_status_text(L"Checking for updates...");
                        ui::set_progress_text(L"Preparing patch flow...");
                    }
                    break;
                }

                patcher::run(
                    kLocalVersionFile,
                    kClientRoot,
                    kRemoteVersionUrl,
                    [](const std::string &version)
                    {
                        versioning::write_local_version(kLocalVersionFile, version);
                    },
                    [](const PatcherProgress &progress)
                    {
                        ui::set_status_text(to_wstring_simple(progress.status));

                        std::wstring progress_line =
                            std::to_wstring(progress.completed_files) + L"/" +
                            std::to_wstring(progress.total_files) + L" files";

                        if (!progress.detail.empty())
                        {
                            progress_line += L" | " + to_wstring_simple(progress.detail);
                        }

                        ui::set_progress_text(progress_line);
                        ui::set_progress_value(progress.overall_percent);
                    });

                finalize_and_launch();
            }
            catch (const std::exception &ex)
            {
                set_error_state(ex.what());
            } })
            .detach();
    }
}

namespace launcher
{
    void launch_game(const std::string &executable_path)
    {
#ifdef _WIN32
        std::wstring exe_path(executable_path.begin(), executable_path.end());

        if (!std::filesystem::exists(exe_path))
        {
            throw std::runtime_error("Game executable not found: " + executable_path);
        }

        std::filesystem::path exe_fs(exe_path);
        const std::wstring working_dir = exe_fs.parent_path().wstring();

        HINSTANCE result = ShellExecuteW(
            nullptr,
            L"open",
            exe_path.c_str(),
            nullptr,
            working_dir.c_str(),
            SW_SHOWNORMAL);

        if ((INT_PTR)result <= 32)
        {
            throw std::runtime_error(
                "Failed to launch game: " + executable_path +
                " | ShellExecute=" + std::to_string((INT_PTR)result));
        }
#else
        throw std::runtime_error("launch_game is only implemented on Windows");
#endif
    }

    void start_patch_flow()
    {
        run_patch_flow(false);
    }

    void start_repair_flow()
    {
        run_patch_flow(true);
    }
}