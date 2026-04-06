#include "../include/patcher.hpp"

#include "../include/comparator.hpp"
#include "../include/file_ops.hpp"
#include "../include/http_client.hpp"
#include "../include/manifest.hpp"
#include "../include/version.hpp"

#include <cstddef>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    int calculate_percent(std::size_t done, std::size_t total)
    {
        if (total == 0)
        {
            return 100;
        }

        return static_cast<int>(
            (100.0 * static_cast<double>(done)) /
            static_cast<double>(total));
    }
}

namespace patcher
{
    void run(
        const std::string &local_version_path,
        const std::string &client_root,
        const std::string &remote_version_url,
        const std::function<void(const std::string &)> &write_local_version,
        ProgressCallback on_progress)
    {
        HttpClient client;
        PatcherProgress progress{};

        auto emit = [&](PatcherState state,
                        const std::string &status,
                        const std::string &detail = std::string())
        {
            progress.state = state;
            progress.status = status;
            progress.detail = detail;

            if (on_progress)
            {
                on_progress(progress);
            }
        };

        try
        {
            emit(PatcherState::CheckingVersion, "Checking local version");

            std::string local_version = "0.0.0";

            try
            {
                local_version = versioning::read_local_version(local_version_path);
            }
            catch (...)
            {
                local_version = "0.0.0";
            }

            emit(
                PatcherState::CheckingVersion,
                "Downloading remote version",
                remote_version_url);

            const std::string remote_version_json = client.get_text(remote_version_url);
            const RemoteVersionInfo remote_info =
                versioning::parse_remote_version_json(remote_version_json);

            emit(
                PatcherState::DownloadingManifest,
                "Downloading manifest",
                remote_info.manifest_url);

            const std::string manifest_json = client.get_text(remote_info.manifest_url);
            const Manifest manifest = manifesting::parse_manifest_json(manifest_json);

            emit(PatcherState::ComparingFiles, "Building patch plan");

            const std::vector<PatchTask> tasks =
                comparator::build_patch_plan(manifest, client_root);

            progress.total_files = tasks.size();
            progress.completed_files = 0;
            progress.current_file_total_bytes = 0.0;
            progress.current_file_downloaded_bytes = 0.0;
            progress.overall_percent = 0;

            const bool version_matches = (local_version == remote_info.version);

            if (version_matches && tasks.empty())
            {
                progress.overall_percent = 100;
                emit(PatcherState::Ready, "Client is up to date");
                return;
            }

            if (tasks.empty())
            {
                emit(PatcherState::Finalizing, "No file changes needed");
                write_local_version(remote_info.version);
                progress.overall_percent = 100;
                emit(PatcherState::Ready, "Update complete");
                return;
            }

            emit(
                PatcherState::DownloadingFiles,
                "Downloading patch files",
                "Files to update: " + std::to_string(tasks.size()));

            for (std::size_t i = 0; i < tasks.size(); ++i)
            {
                const PatchTask &task = tasks[i];

                progress.completed_files = i;
                progress.current_file_total_bytes = static_cast<double>(task.file.size);
                progress.current_file_downloaded_bytes = 0.0;
                progress.overall_percent = calculate_percent(i, tasks.size());

                emit(
                    PatcherState::DownloadingFiles,
                    "Downloading file",
                    task.remote_url);

                client.download_to_file(
                    task.remote_url,
                    task.local_path,
                    task.file.size,
                    [&](double total_bytes, double downloaded_bytes)
                    {
                        progress.current_file_total_bytes = total_bytes;
                        progress.current_file_downloaded_bytes = downloaded_bytes;

                        double file_ratio = 0.0;
                        if (total_bytes > 0.0)
                        {
                            file_ratio = downloaded_bytes / total_bytes;
                        }

                        if (file_ratio < 0.0)
                        {
                            file_ratio = 0.0;
                        }
                        if (file_ratio > 1.0)
                        {
                            file_ratio = 1.0;
                        }

                        const double overall =
                            (static_cast<double>(i) + file_ratio) /
                            static_cast<double>(tasks.size());

                        progress.overall_percent = static_cast<int>(overall * 100.0);

                        if (on_progress)
                        {
                            on_progress(progress);
                        }
                    });

                const std::uint64_t final_size = file_ops::get_file_size(task.local_path);
                if (final_size != task.file.size)
                {
                    throw std::runtime_error(
                        "Final file size mismatch for: " + task.file.path +
                        " expected=" + std::to_string(task.file.size) +
                        " got=" + std::to_string(final_size) +
                        " | URL: " + task.remote_url);
                }

                progress.completed_files = i + 1;
                progress.current_file_total_bytes = 0.0;
                progress.current_file_downloaded_bytes = 0.0;
                progress.overall_percent = calculate_percent(i + 1, tasks.size());

                emit(
                    PatcherState::DownloadingFiles,
                    "Downloaded file",
                    task.file.path);
            }

            progress.completed_files = tasks.size();
            progress.current_file_total_bytes = 0.0;
            progress.current_file_downloaded_bytes = 0.0;
            progress.overall_percent = 100;

            emit(PatcherState::Finalizing, "Writing local version");

            write_local_version(remote_info.version);

            emit(PatcherState::Ready, "Patch complete");
        }
        catch (const std::exception &ex)
        {
            progress.state = PatcherState::Error;
            progress.status = "Patch failed";
            progress.detail = ex.what();

            if (on_progress)
            {
                on_progress(progress);
            }

            throw;
        }
    }
}