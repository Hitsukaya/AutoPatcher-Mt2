#pragma once

#include <cstddef>
#include <functional>
#include <string>

enum class PatcherState
{
    Idle,
    CheckingVersion,
    DownloadingManifest,
    ComparingFiles,
    DownloadingFiles,
    Finalizing,
    Ready,
    Error
};

struct PatcherProgress
{
    PatcherState state = PatcherState::Idle;
    std::string status;
    std::string detail;

    std::size_t total_files = 0;
    std::size_t completed_files = 0;

    double current_file_total_bytes = 0.0;
    double current_file_downloaded_bytes = 0.0;

    int overall_percent = 0;
};

using ProgressCallback = std::function<void(const PatcherProgress &)>;

namespace patcher
{
    void run(
        const std::string &local_version_path,
        const std::string &client_root,
        const std::string &remote_version_url,
        const std::function<void(const std::string &)> &write_local_version,
        ProgressCallback on_progress);
}