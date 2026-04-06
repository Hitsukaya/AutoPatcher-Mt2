#include "../include/version.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
    std::string trim(const std::string &input)
    {
        std::string value = input;

        value.erase(
            value.begin(),
            std::find_if(value.begin(), value.end(), [](unsigned char ch)
                         { return !std::isspace(ch); }));

        value.erase(
            std::find_if(value.rbegin(), value.rend(), [](unsigned char ch)
                         { return !std::isspace(ch); })
                .base(),
            value.end());

        return value;
    }
}

namespace versioning
{
    std::string read_local_version(const std::string &path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            throw std::runtime_error("Cannot open local version file: " + path);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        const std::string version = trim(buffer.str());
        if (version.empty())
        {
            throw std::runtime_error("Local version file is empty: " + path);
        }

        return version;
    }

    void write_local_version(const std::string &path, const std::string &version)
    {
        std::ofstream file(path, std::ios::trunc);
        if (!file.is_open())
        {
            throw std::runtime_error("Cannot write local version file: " + path);
        }

        file << version;
        if (!file.good())
        {
            throw std::runtime_error("Failed writing local version file: " + path);
        }
    }

    RemoteVersionInfo parse_remote_version_json(const std::string &json_text)
    {
        const auto json = nlohmann::json::parse(json_text);

        RemoteVersionInfo info;
        info.version = json.at("version").get<std::string>();
        info.manifest_url = json.at("manifest_url").get<std::string>();

        if (info.version.empty())
        {
            throw std::runtime_error("Remote version is empty");
        }

        if (info.manifest_url.empty())
        {
            throw std::runtime_error("Remote manifest_url is empty");
        }

        return info;
    }
}