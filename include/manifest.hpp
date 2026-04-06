#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct ManifestEntry
{
    std::string path;
    std::uint64_t size = 0;
};

struct Manifest
{
    std::string base_url;
    std::vector<ManifestEntry> files;
};

namespace manifesting
{
    Manifest parse_manifest_json(const std::string &json_text);
}