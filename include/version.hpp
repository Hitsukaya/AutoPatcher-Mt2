#pragma once

#include <string>

struct RemoteVersionInfo
{
    std::string version;
    std::string manifest_url;
};

namespace versioning
{
    std::string read_local_version(const std::string &path);
    void write_local_version(const std::string &path, const std::string &version);
    RemoteVersionInfo parse_remote_version_json(const std::string &json_text);
}