#pragma once

#include <cstdint>
#include <string>

namespace file_ops
{
    bool exists(const std::string &path);
    void ensure_parent_directory(const std::string &file_path);
    std::uint64_t get_file_size(const std::string &path);
    void remove_file(const std::string &path);
}