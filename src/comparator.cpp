#include "../include/comparator.hpp"
#include "../include/file_ops.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    std::string join_url(const std::string &base, const std::string &relative)
    {
        if (base.empty())
        {
            return relative;
        }

        if (base.back() == '/')
        {
            return base + relative;
        }

        return base + "/" + relative;
    }

    std::string join_path(const std::string &root, const std::string &relative)
    {
        fs::path full = fs::path(root) / fs::path(relative);
        return full.lexically_normal().string();
    }
}

namespace comparator
{
    std::vector<PatchTask> build_patch_plan(
        const Manifest &manifest,
        const std::string &client_root)
    {
        std::vector<PatchTask> tasks;

        for (const auto &file : manifest.files)
        {
            const std::string local_path = join_path(client_root, file.path);
            const std::string remote_url = join_url(manifest.base_url, file.path);

            if (!file_ops::exists(local_path))
            {
                tasks.push_back(PatchTask{
                    file,
                    PatchReason::Missing,
                    local_path,
                    remote_url});
                continue;
            }

            const std::uint64_t local_size = file_ops::get_file_size(local_path);

            if (local_size != file.size)
            {
                tasks.push_back(PatchTask{
                    file,
                    PatchReason::SizeMismatch,
                    local_path,
                    remote_url});
                continue;
            }
        }

        return tasks;
    }

    std::string reason_to_string(PatchReason reason)
    {
        switch (reason)
        {
        case PatchReason::Missing:
            return "missing";

        case PatchReason::SizeMismatch:
            return "size_mismatch";

        default:
            return "unknown";
        }
    }
}