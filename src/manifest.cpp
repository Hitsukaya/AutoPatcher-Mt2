#include "../include/manifest.hpp"

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>

namespace manifesting
{
    Manifest parse_manifest_json(const std::string &json_text)
    {
        const nlohmann::json json = nlohmann::json::parse(json_text);

        Manifest manifest;

        if (!json.contains("base_url") || !json["base_url"].is_string())
        {
            throw std::runtime_error("Manifest is missing valid 'base_url'");
        }

        manifest.base_url = json["base_url"].get<std::string>();

        if (manifest.base_url.empty())
        {
            throw std::runtime_error("Manifest 'base_url' is empty");
        }

        if (!json.contains("files") || !json["files"].is_array())
        {
            throw std::runtime_error("Manifest is missing valid 'files' array");
        }

        for (const auto &item : json["files"])
        {
            if (!item.is_object())
            {
                throw std::runtime_error("Manifest file entry is not an object");
            }

            if (!item.contains("path") || !item["path"].is_string())
            {
                throw std::runtime_error("Manifest file entry is missing valid 'path'");
            }

            if (!item.contains("size") || !item["size"].is_number_unsigned())
            {
                throw std::runtime_error("Manifest file entry is missing valid 'size'");
            }

            ManifestEntry entry;
            entry.path = item["path"].get<std::string>();
            entry.size = item["size"].get<std::uint64_t>();

            if (entry.path.empty())
            {
                throw std::runtime_error("Manifest file entry has empty 'path'");
            }

            manifest.files.push_back(entry);
        }

        if (manifest.files.empty())
        {
            throw std::runtime_error("Manifest file list is empty");
        }

        return manifest;
    }
}