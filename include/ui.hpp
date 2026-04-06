#pragma once

#include <string>
#include <windows.h>

namespace ui
{
    bool register_window_class(HINSTANCE instance);
    HWND create_main_window(HINSTANCE instance, int show_cmd);

    void set_status_text(const std::wstring &text);
    void set_progress_text(const std::wstring &text);
    void set_progress_value(int value);
    void set_start_enabled(bool enabled);
    void set_controls_enabled(bool enabled);
    void set_hero_title(const std::wstring &text);
    void set_hero_subtitle(const std::wstring &text);

    std::wstring game_path();
    std::wstring config_path();
    std::wstring preview_url();

    HWND main_window();
}