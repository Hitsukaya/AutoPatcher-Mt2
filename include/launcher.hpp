#pragma once

#include <string>

namespace launcher
{
    void launch_game(const std::string &executable_path);
    void start_patch_flow();
    void start_repair_flow();
}