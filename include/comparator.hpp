#pragma once

#include "manifest.hpp"

#include <string>
#include <vector>

enum class PatchReason
{
    Missing,
    SizeMismatch
};

struct PatchTask
{
    ManifestEntry file;
    PatchReason reason;
    std::string local_path;
    std::string remote_url;
};

namespace comparator
{
    std::vector<PatchTask> build_patch_plan(
        const Manifest &manifest,
        const std::string &client_root);

    std::string reason_to_string(PatchReason reason);
}