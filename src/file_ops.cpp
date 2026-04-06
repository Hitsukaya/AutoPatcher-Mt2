#include "../include/file_ops.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace file_ops
{
    bool exists(const std::string &path)
    {
        std::error_code ec;
        return fs::exists(path, ec) && !ec;
    }

    void ensure_parent_directory(const std::string &file_path)
    {
        fs::path p(file_path);
        fs::path parent = p.parent_path();

        if (parent.empty())
        {
            return;
        }

        std::error_code ec;
        fs::create_directories(parent, ec);

        if (ec)
        {
            throw std::runtime_error(
                "Failed to create directory: " + parent.string());
        }
    }

    std::uint64_t get_file_size(const std::string &path)
    {
        std::error_code ec;
        const auto size = fs::file_size(path, ec);

        if (ec)
        {
            throw std::runtime_error("Failed to get file size: " + path);
        }

        return static_cast<std::uint64_t>(size);
    }

    void remove_file(const std::string &path)
    {
        std::error_code ec;
        fs::remove(path, ec);

        if (ec)
        {
            throw std::runtime_error("Failed to remove file: " + path);
        }
    }
}