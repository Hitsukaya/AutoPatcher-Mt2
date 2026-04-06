#include "../include/ui.hpp"
#include "../include/launcher.hpp"

#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <gdiplus.h>
#include <wrl.h>
#include <WebView2.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "uxtheme.lib")

using namespace Gdiplus;
using Microsoft::WRL::ComPtr;

namespace
{
    constexpr wchar_t kWindowClassName[] = L"Nyx2LauncherWindow";
    constexpr int kAppIconId = 101;

    std::wstring utf8_to_wstring(const std::string &text)
    {
        if (text.empty())
            return L"";

        int size_needed = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0);
        if (size_needed <= 0)
            return std::wstring(text.begin(), text.end());

        std::wstring result(size_needed, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), &result[0], size_needed);
        return result;
    }

    std::wstring path_to_file_url(const std::wstring &path)
    {
        std::wstring normalized = std::filesystem::path(path).lexically_normal().wstring();

        for (auto &ch : normalized)
        {
            if (ch == L'\\')
                ch = L'/';
        }

        if (!normalized.empty() && normalized[0] != L'/')
            normalized = L"/" + normalized;

        return L"file://" + normalized;
    }

    bool read_text_file_utf8(const std::wstring &path, std::wstring &out)
    {
        HANDLE file = CreateFileW(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (file == INVALID_HANDLE_VALUE)
            return false;

        LARGE_INTEGER size{};
        if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0)
        {
            CloseHandle(file);
            return false;
        }

        std::string buffer;
        buffer.resize(static_cast<size_t>(size.QuadPart));

        DWORD bytes_read = 0;
        BOOL ok = ReadFile(file, buffer.data(), (DWORD)buffer.size(), &bytes_read, nullptr);
        CloseHandle(file);

        if (!ok)
            return false;

        if (bytes_read != buffer.size())
            buffer.resize(bytes_read);

        if (buffer.size() >= 3 &&
            (unsigned char)buffer[0] == 0xEF &&
            (unsigned char)buffer[1] == 0xBB &&
            (unsigned char)buffer[2] == 0xBF)
        {
            buffer.erase(0, 3);
        }

        out = utf8_to_wstring(buffer);
        return true;
    }

    LRESULT CALLBACK button_subclass_proc(
        HWND hwnd,
        UINT msg,
        WPARAM wparam,
        LPARAM lparam,
        UINT_PTR subclass_id,
        DWORD_PTR ref_data)
    {
        switch (msg)
        {
        case WM_ERASEBKGND:
            return 1;

        case WM_NCPAINT:
            return 0;

        case WM_PRINTCLIENT:
            return 0;
        }

        return DefSubclassProc(hwnd, msg, wparam, lparam);
    }

    enum ControlIds
    {
        IDC_START = 1001,
        IDC_REPAIR = 1002,
        IDC_CONFIG = 1003,
        IDC_ACCOUNT = 1004,
        IDC_COMMUNITY = 1005,
        IDC_MINIMIZE = 1006,
        IDC_CLOSE = 1007,
        IDC_PREVIEW_HOST = 1008
    };

    struct Theme
    {
        COLORREF app_bg = RGB(8, 10, 14);
        COLORREF app_bg_alt = RGB(12, 16, 22);

        COLORREF topbar_fill = RGB(10, 12, 18);
        COLORREF topbar_line = RGB(42, 60, 86);

        COLORREF hero_shell = RGB(14, 18, 24);
        COLORREF hero_shell_line = RGB(48, 70, 100);

        COLORREF overlay_fill = RGB(18, 22, 30);
        COLORREF overlay_line = RGB(80, 110, 150);

        COLORREF side_panel_fill = RGB(12, 16, 22);
        COLORREF side_panel_line = RGB(44, 62, 88);

        COLORREF footer_fill = RGB(10, 14, 18);
        COLORREF footer_line = RGB(42, 58, 82);

        COLORREF start_fill = RGB(50, 120, 255);
        COLORREF start_hover = RGB(78, 142, 255);
        COLORREF start_pressed = RGB(34, 96, 218);

        COLORREF repair_fill = RGB(130, 84, 30);
        COLORREF repair_hover = RGB(156, 102, 42);
        COLORREF repair_pressed = RGB(106, 66, 22);

        COLORREF secondary_fill = RGB(20, 26, 34);
        COLORREF secondary_hover = RGB(30, 38, 50);
        COLORREF secondary_pressed = RGB(16, 22, 30);

        COLORREF top_button_fill = RGB(22, 28, 36);
        COLORREF top_button_hover = RGB(34, 42, 52);
        COLORREF top_button_close = RGB(120, 38, 50);
        COLORREF top_button_close_hover = RGB(150, 48, 62);

        COLORREF progress_bg = RGB(16, 20, 28);
        COLORREF progress_fill = RGB(56, 126, 255);
        COLORREF progress_line = RGB(48, 68, 96);

        COLORREF text = RGB(244, 247, 252);
        COLORREF text_soft = RGB(206, 216, 230);
        COLORREF text_muted = RGB(130, 146, 168);
        COLORREF text_accent = RGB(110, 170, 255);

        int window_width = 1500;
        int window_height = 900;

        int topbar_h = 72;
        int outer_pad = 22;
        int footer_h = 68;

        int window_radius = 22;
        int hero_radius = 22;
        int panel_radius = 16;
        int footer_radius = 16;
        int button_radius = 10;
        int primary_radius = 12;
        int top_button_radius = 8;
    };

    struct LauncherConfig
    {
        std::wstring window_title;
        std::wstring logo_text;
        std::wstring topbar_subtitle;

        std::wstring background_path;
        std::wstring icon_path;

        std::wstring hero_title;
        std::wstring hero_subtitle;

        std::wstring preview_url;

        std::wstring account_url;
        std::wstring community_url;

        std::wstring game_path;
        std::wstring config_path;

        std::wstring initial_status_text;
        std::wstring initial_progress_text;
        bool start_enabled = true;
    };

    struct LayoutRects
    {
        RECT topbar{};

        RECT hero_shell{};
        RECT hero{};
        RECT hero_overlay{};

        RECT side_panel{};

        RECT btn_start{};
        RECT btn_repair{};
        RECT btn_config{};
        RECT btn_account{};
        RECT btn_community{};

        RECT btn_minimize{};
        RECT btn_close{};

        RECT footer{};
        RECT footer_status{};
        RECT footer_progress_label{};
        RECT footer_progress{};
        RECT footer_percent{};
    };

    Theme g_theme{};
    LauncherConfig g_config{};
    LayoutRects g_layout{};

    HWND g_main_window = nullptr;
    HWND g_preview_host = nullptr;
    HWND g_start_button = nullptr;
    HWND g_repair_button = nullptr;
    HWND g_config_button = nullptr;
    HWND g_account_button = nullptr;
    HWND g_community_button = nullptr;
    HWND g_minimize_button = nullptr;
    HWND g_close_button = nullptr;

    HFONT g_logo_font = nullptr;
    HFONT g_logo_sub_font = nullptr;
    HFONT g_title_font = nullptr;
    HFONT g_subtitle_font = nullptr;
    HFONT g_small_font = nullptr;
    HFONT g_micro_font = nullptr;
    HFONT g_button_font = nullptr;
    HFONT g_button_large_font = nullptr;
    HFONT g_top_button_font = nullptr;
    HFONT g_footer_font = nullptr;

    HBRUSH g_background_brush = nullptr;
    HBRUSH g_preview_brush = nullptr;

    ULONG_PTR g_gdiplus_token = 0;
    std::unique_ptr<Image> g_background_image;
    bool g_background_loaded = false;

    ComPtr<ICoreWebView2Controller> g_webview_controller;
    ComPtr<ICoreWebView2> g_webview;
    bool g_webview_ready = false;
    bool g_webview_initializing = false;

    int g_progress_value = 0;
    std::wstring g_status_text = L"Launcher ready.";
    std::wstring g_progress_text = L"Waiting for patch check...";

    RECT make_rect(int l, int t, int r, int b)
    {
        RECT rc{l, t, r, b};
        return rc;
    }

    Color to_color(COLORREF rgb, BYTE alpha = 255)
    {
        return Color(alpha, GetRValue(rgb), GetGValue(rgb), GetBValue(rgb));
    }

    std::wstring get_exe_directory()
    {
        wchar_t buffer[MAX_PATH]{};
        GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        std::filesystem::path exe_path(buffer);
        return exe_path.parent_path().wstring();
    }

    std::wstring absolute_from_exe(const std::wstring &relative_or_absolute)
    {
        std::filesystem::path p(relative_or_absolute);
        if (p.is_absolute())
            return p.lexically_normal().wstring();

        std::filesystem::path base = get_exe_directory();
        base /= p;
        return base.lexically_normal().wstring();
    }

    bool file_exists(const std::wstring &path)
    {
        DWORD attr = GetFileAttributesW(path.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
    }

    std::wstring read_ini_string(const std::wstring &ini_path,
                                 const wchar_t *section,
                                 const wchar_t *key,
                                 const wchar_t *fallback = L"")
    {
        wchar_t buffer[4096]{};
        GetPrivateProfileStringW(section, key, fallback, buffer, 4096, ini_path.c_str());
        return buffer;
    }

    bool parse_ini_bool(const std::wstring &value, bool fallback)
    {
        if (value == L"1" || value == L"true" || value == L"TRUE" || value == L"yes" || value == L"YES")
            return true;
        if (value == L"0" || value == L"false" || value == L"FALSE" || value == L"no" || value == L"NO")
            return false;
        return fallback;
    }

    void load_default_config()
    {
        g_config.window_title = L"Nyx2 Launcher";
        g_config.logo_text = L"NYX2";
        g_config.topbar_subtitle = L"Official Game Launcher";

        g_config.background_path = absolute_from_exe(L"assets/background.jpg");
        g_config.icon_path = absolute_from_exe(L"assets/metin2.ico");

        g_config.hero_title = L"ENTER THE WORLD OF NYX2";
        g_config.hero_subtitle = L"Fast patching, stable launcher flow and instant client access.";

        g_config.preview_url = L"https://nyx2.hitsukaya.com/launcher-preview.html";
        g_config.account_url = L"https://your-domain.com/account";
        g_config.community_url = L"https://your-domain.com/community";

        g_config.game_path = absolute_from_exe(L"client/Nyx2.exe");
        g_config.config_path = absolute_from_exe(L"client/config.exe");

        g_config.initial_status_text = L"Launcher ready.";
        g_config.initial_progress_text = L"Waiting for patch check...";
        g_config.start_enabled = true;
    }

    void load_launcher_config()
    {
        load_default_config();

        const std::wstring ini_path = absolute_from_exe(L"launcher.ini");
        if (!file_exists(ini_path))
            return;

        g_config.window_title = read_ini_string(ini_path, L"branding", L"window_title", g_config.window_title.c_str());
        g_config.logo_text = read_ini_string(ini_path, L"branding", L"logo", g_config.logo_text.c_str());
        g_config.topbar_subtitle = read_ini_string(ini_path, L"branding", L"topbar_subtitle", g_config.topbar_subtitle.c_str());

        g_config.background_path.clear();
        g_config.icon_path = absolute_from_exe(read_ini_string(ini_path, L"assets", L"icon", L"assets/metin2.ico"));

        g_config.hero_title = read_ini_string(ini_path, L"hero", L"title", g_config.hero_title.c_str());
        g_config.hero_subtitle = read_ini_string(ini_path, L"hero", L"subtitle", g_config.hero_subtitle.c_str());

        g_config.preview_url = read_ini_string(ini_path, L"preview", L"url", g_config.preview_url.c_str());
        g_config.account_url = read_ini_string(ini_path, L"links", L"account", g_config.account_url.c_str());
        g_config.community_url = read_ini_string(ini_path, L"links", L"community", g_config.community_url.c_str());

        g_config.game_path = absolute_from_exe(read_ini_string(ini_path, L"client", L"game", L"client/Nyx2.exe"));
        g_config.config_path = absolute_from_exe(read_ini_string(ini_path, L"client", L"config", L"client/config.exe"));

        g_config.initial_status_text = read_ini_string(ini_path, L"status", L"initial_status", g_config.initial_status_text.c_str());
        g_config.initial_progress_text = read_ini_string(ini_path, L"status", L"initial_progress", g_config.initial_progress_text.c_str());
        g_config.start_enabled = parse_ini_bool(read_ini_string(ini_path, L"status", L"start_enabled", L"true"), true);
    }

    void initialize_gdiplus()
    {
        if (g_gdiplus_token != 0)
            return;

        GdiplusStartupInput input;
        GdiplusStartup(&g_gdiplus_token, &input, nullptr);
    }

    void shutdown_gdiplus()
    {
        if (g_gdiplus_token != 0)
        {
            GdiplusShutdown(g_gdiplus_token);
            g_gdiplus_token = 0;
        }
    }

    void load_assets()
    {
        initialize_gdiplus();
        g_background_loaded = false;
        g_background_image.reset();
    }

    void create_fonts()
    {
        if (!g_logo_font)
            g_logo_font = CreateFontW(38, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

        if (!g_logo_sub_font)
            g_logo_sub_font = CreateFontW(14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

        if (!g_title_font)
            g_title_font = CreateFontW(34, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                                       CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

        if (!g_subtitle_font)
            g_subtitle_font = CreateFontW(17, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

        if (!g_small_font)
            g_small_font = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                                       CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

        if (!g_micro_font)
            g_micro_font = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                                       CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

        if (!g_button_font)
            g_button_font = CreateFontW(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

        if (!g_button_large_font)
            g_button_large_font = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                              DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                                              CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

        if (!g_top_button_font)
            g_top_button_font = CreateFontW(11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                            DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                                            CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

        if (!g_footer_font)
            g_footer_font = CreateFontW(13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
    }

    void destroy_fonts()
    {
        if (g_logo_font)
            DeleteObject(g_logo_font);
        if (g_logo_sub_font)
            DeleteObject(g_logo_sub_font);
        if (g_title_font)
            DeleteObject(g_title_font);
        if (g_subtitle_font)
            DeleteObject(g_subtitle_font);
        if (g_small_font)
            DeleteObject(g_small_font);
        if (g_micro_font)
            DeleteObject(g_micro_font);
        if (g_button_font)
            DeleteObject(g_button_font);
        if (g_button_large_font)
            DeleteObject(g_button_large_font);
        if (g_top_button_font)
            DeleteObject(g_top_button_font);
        if (g_footer_font)
            DeleteObject(g_footer_font);

        g_logo_font = nullptr;
        g_logo_sub_font = nullptr;
        g_title_font = nullptr;
        g_subtitle_font = nullptr;
        g_small_font = nullptr;
        g_micro_font = nullptr;
        g_button_font = nullptr;
        g_button_large_font = nullptr;
        g_top_button_font = nullptr;
        g_footer_font = nullptr;
    }

    void disable_button_theme(HWND hwnd)
    {
        if (hwnd)
            SetWindowTheme(hwnd, L"", L"");
    }

    void apply_window_region(HWND hwnd)
    {
        RECT rc{};
        GetWindowRect(hwnd, &rc);

        HRGN region = CreateRoundRectRgn(
            0,
            0,
            (rc.right - rc.left) + 1,
            (rc.bottom - rc.top) + 1,
            g_theme.window_radius,
            g_theme.window_radius);

        SetWindowRgn(hwnd, region, TRUE);
    }

    void build_round_path(GraphicsPath &path, const RECT &rect, int radius)
    {
        const int width = rect.right - rect.left;
        const int height = rect.bottom - rect.top;
        if (width <= 0 || height <= 0)
            return;

        int r = radius < 2 ? 2 : radius;
        int d = r * 2;
        if (d > width)
            d = width;
        if (d > height)
            d = height;

        const REAL left = static_cast<REAL>(rect.left);
        const REAL top = static_cast<REAL>(rect.top);
        const REAL right = static_cast<REAL>(rect.right);
        const REAL bottom = static_cast<REAL>(rect.bottom);
        const REAL dia = static_cast<REAL>(d);

        path.Reset();
        path.AddArc(left, top, dia, dia, 180.0f, 90.0f);
        path.AddArc(right - dia, top, dia, dia, 270.0f, 90.0f);
        path.AddArc(right - dia, bottom - dia, dia, dia, 0.0f, 90.0f);
        path.AddArc(left, bottom - dia, dia, dia, 90.0f, 90.0f);
        path.CloseFigure();
    }

    void fill_rounded_panel(Graphics &g,
                            const RECT &rect,
                            COLORREF fill,
                            COLORREF border,
                            int radius,
                            BYTE alpha_fill = 255,
                            BYTE alpha_border = 255,
                            float pen_width = 1.0f)
    {
        GraphicsPath path;
        build_round_path(path, rect, radius);

        SolidBrush brush(to_color(fill, alpha_fill));
        Pen pen(to_color(border, alpha_border), pen_width);

        g.FillPath(&brush, &path);
        if (alpha_border > 0 && pen_width > 0.0f)
            g.DrawPath(&pen, &path);
    }

    void calculate_layout(HWND hwnd)
    {
        RECT client{};
        GetClientRect(hwnd, &client);

        const int cw = client.right;
        const int ch = client.bottom;

        g_layout.topbar = make_rect(0, 0, cw, g_theme.topbar_h);

        const int left = g_theme.outer_pad;
        const int right = cw - g_theme.outer_pad;
        const int top = g_theme.topbar_h + 16;
        const int bottom = ch - g_theme.outer_pad;

        g_layout.footer = make_rect(left, bottom - g_theme.footer_h, right, bottom);

        const int content_top = top;
        const int content_bottom = g_layout.footer.top - 14;
        const int side_w = 184;
        const int gap = 14;

        const int hero_top = content_top + 8;
        const int hero_bottom = content_bottom - 8;
        const int hero_height = hero_bottom - hero_top;

        const int panel_height = 250;
        const int panel_top = hero_top + 18;

        g_layout.side_panel = make_rect(
            right - side_w,
            panel_top,
            right,
            panel_top + panel_height);

        g_layout.hero_shell = make_rect(
            left + 18,
            hero_top,
            g_layout.side_panel.left - gap,
            hero_bottom);

        g_layout.hero = make_rect(
            g_layout.hero_shell.left + 14,
            g_layout.hero_shell.top + 14,
            g_layout.hero_shell.right - 14,
            g_layout.hero_shell.bottom - 14);

        g_layout.hero_overlay = make_rect(
            g_layout.hero.left + 30,
            g_layout.hero.top + 28,
            g_layout.hero.left + 540,
            g_layout.hero.top + 176);

        const int btn_left = g_layout.side_panel.left + 14;
        const int btn_right = g_layout.side_panel.right - 14;
        int y = g_layout.side_panel.top + 28;

        g_layout.btn_start = make_rect(btn_left, y, btn_right, y + 38);
        y += 38 + 8;

        g_layout.btn_repair = make_rect(btn_left, y, btn_right, y + 38);
        y += 38 + 10;

        g_layout.btn_config = make_rect(btn_left, y, btn_right, y + 30);
        y += 30 + 6;

        g_layout.btn_account = make_rect(btn_left, y, btn_right, y + 30);
        y += 30 + 6;

        g_layout.btn_community = make_rect(btn_left, y, btn_right, y + 30);

        g_layout.btn_minimize = make_rect(cw - 84, 18, cw - 48, 40);
        g_layout.btn_close = make_rect(cw - 44, 18, cw - 8, 40);

        g_layout.footer_status = make_rect(
            g_layout.footer.left + 18,
            g_layout.footer.top + 8,
            g_layout.footer.left + 240,
            g_layout.footer.bottom - 8);

        g_layout.footer_progress_label = make_rect(
            g_layout.footer.left + 250,
            g_layout.footer.top + 8,
            g_layout.footer.left + 880,
            g_layout.footer.top + 22);

        g_layout.footer_progress = make_rect(
            g_layout.footer.left + 250,
            g_layout.footer.top + 26,
            g_layout.footer.right - 60,
            g_layout.footer.bottom - 12);

        g_layout.footer_percent = make_rect(
            g_layout.footer.right - 54,
            g_layout.footer.top + 10,
            g_layout.footer.right - 10,
            g_layout.footer.bottom - 10);
    }

    void resize_webview()
    {
        if (g_preview_host)
        {
            MoveWindow(
                g_preview_host,
                g_layout.hero.left,
                g_layout.hero.top,
                g_layout.hero.right - g_layout.hero.left,
                g_layout.hero.bottom - g_layout.hero.top,
                TRUE);
        }

        if (g_webview_controller && g_preview_host)
        {
            RECT bounds{};
            GetClientRect(g_preview_host, &bounds);
            g_webview_controller->put_Bounds(bounds);
            g_webview_controller->NotifyParentWindowPositionChanged();
            g_webview_controller->put_IsVisible(TRUE);
        }
    }

    void initialize_webview()
    {
        if (!g_preview_host || g_webview_ready || g_webview_initializing)
            return;

        g_webview_initializing = true;

        HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
            nullptr,
            nullptr,
            nullptr,
            Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [](HRESULT env_hr, ICoreWebView2Environment *env) -> HRESULT
                {
                    g_webview_initializing = false;

                    if (FAILED(env_hr) || !env)
                    {
                        return S_OK;
                    }

                    return env->CreateCoreWebView2Controller(
                        g_preview_host,
                        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [](HRESULT controller_hr, ICoreWebView2Controller *controller) -> HRESULT
                            {
                                if (FAILED(controller_hr) || !controller)
                                    return S_OK;

                                g_webview_controller = controller;
                                g_webview_controller->get_CoreWebView2(&g_webview);

                                if (!g_webview)
                                    return S_OK;

                                g_webview_ready = true;
                                g_webview_controller->put_IsVisible(TRUE);

                                ComPtr<ICoreWebView2Settings> settings;
                                if (SUCCEEDED(g_webview->get_Settings(&settings)) && settings)
                                {
                                    settings->put_IsStatusBarEnabled(FALSE);
                                    settings->put_AreDevToolsEnabled(TRUE);
                                    settings->put_IsZoomControlEnabled(FALSE);
                                    settings->put_AreDefaultContextMenusEnabled(TRUE);
                                }

                                resize_webview();

                                std::wstring target = g_config.preview_url;
                                if (target.empty())
                                    target = L"launcher-preview.html";

                                if (target.rfind(L"http://", 0) == 0 || target.rfind(L"https://", 0) == 0)
                                {
                                    HRESULT nav_hr = g_webview->Navigate(target.c_str());
                                    if (FAILED(nav_hr))
                                    {
                                        std::wstring msg =
                                            L"Navigate failed.\nHRESULT = " +
                                            std::to_wstring((long long)nav_hr);
                                        MessageBoxW(g_main_window, msg.c_str(), L"WebView2 Error", MB_OK | MB_ICONERROR);
                                    }
                                }
                                else
                                {
                                    std::wstring full_path = absolute_from_exe(target);

                                    if (!file_exists(full_path))
                                    {
                                        std::wstring msg = L"Preview file not found:\n" + full_path;
                                        MessageBoxW(g_main_window, msg.c_str(), L"Preview Error", MB_OK | MB_ICONERROR);
                                        return S_OK;
                                    }

                                    std::wstring file_url = path_to_file_url(full_path);

                                    HRESULT nav_hr = g_webview->Navigate(file_url.c_str());
                                    if (FAILED(nav_hr))
                                    {
                                        std::wstring msg =
                                            L"Navigate local file failed.\nHRESULT = " +
                                            std::to_wstring((long long)nav_hr) +
                                            L"\n\nPath:\n" + full_path +
                                            L"\n\nURL:\n" + file_url;

                                        MessageBoxW(g_main_window, msg.c_str(), L"WebView2 Error", MB_OK | MB_ICONERROR);
                                    }
                                }

                                return S_OK;
                            })
                            .Get());
                })
                .Get());

        if (FAILED(hr))
        {
            g_webview_initializing = false;
        }
    }
    void move_controls()
    {
        auto set_pos = [](HWND h, const RECT &r)
        {
            if (h)
                MoveWindow(h, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
        };

        set_pos(g_start_button, g_layout.btn_start);
        set_pos(g_repair_button, g_layout.btn_repair);
        set_pos(g_config_button, g_layout.btn_config);
        set_pos(g_account_button, g_layout.btn_account);
        set_pos(g_community_button, g_layout.btn_community);
        set_pos(g_minimize_button, g_layout.btn_minimize);
        set_pos(g_close_button, g_layout.btn_close);

        resize_webview();
    }

    void apply_fonts()
    {
        SendMessageW(g_start_button, WM_SETFONT, reinterpret_cast<WPARAM>(g_button_large_font), TRUE);
        SendMessageW(g_repair_button, WM_SETFONT, reinterpret_cast<WPARAM>(g_button_large_font), TRUE);
        SendMessageW(g_config_button, WM_SETFONT, reinterpret_cast<WPARAM>(g_button_font), TRUE);
        SendMessageW(g_account_button, WM_SETFONT, reinterpret_cast<WPARAM>(g_button_font), TRUE);
        SendMessageW(g_community_button, WM_SETFONT, reinterpret_cast<WPARAM>(g_button_font), TRUE);
        SendMessageW(g_minimize_button, WM_SETFONT, reinterpret_cast<WPARAM>(g_top_button_font), TRUE);
        SendMessageW(g_close_button, WM_SETFONT, reinterpret_cast<WPARAM>(g_top_button_font), TRUE);
    }

    void create_controls(HWND hwnd)
    {
        const DWORD button_style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW;
        const DWORD preview_style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

        g_preview_host = CreateWindowExW(
            0,
            L"STATIC",
            L"",
            preview_style,
            0, 0, 0, 0,
            hwnd,
            reinterpret_cast<HMENU>(IDC_PREVIEW_HOST),
            nullptr,
            nullptr);

        g_start_button = CreateWindowExW(
            0, L"BUTTON", L"PLAY", button_style,
            0, 0, 0, 0,
            hwnd, reinterpret_cast<HMENU>(IDC_START), nullptr, nullptr);

        g_repair_button = CreateWindowExW(
            0, L"BUTTON", L"REPAIR", button_style,
            0, 0, 0, 0,
            hwnd, reinterpret_cast<HMENU>(IDC_REPAIR), nullptr, nullptr);

        g_config_button = CreateWindowExW(
            0, L"BUTTON", L"SETTINGS", button_style,
            0, 0, 0, 0,
            hwnd, reinterpret_cast<HMENU>(IDC_CONFIG), nullptr, nullptr);

        g_account_button = CreateWindowExW(
            0, L"BUTTON", L"ACCOUNT", button_style,
            0, 0, 0, 0,
            hwnd, reinterpret_cast<HMENU>(IDC_ACCOUNT), nullptr, nullptr);

        g_community_button = CreateWindowExW(
            0, L"BUTTON", L"COMMUNITY", button_style,
            0, 0, 0, 0,
            hwnd, reinterpret_cast<HMENU>(IDC_COMMUNITY), nullptr, nullptr);

        g_minimize_button = CreateWindowExW(
            0, L"BUTTON", L"-", button_style,
            0, 0, 0, 0,
            hwnd, reinterpret_cast<HMENU>(IDC_MINIMIZE), nullptr, nullptr);

        g_close_button = CreateWindowExW(
            0, L"BUTTON", L"X", button_style,
            0, 0, 0, 0,
            hwnd, reinterpret_cast<HMENU>(IDC_CLOSE), nullptr, nullptr);

        disable_button_theme(g_start_button);
        disable_button_theme(g_repair_button);
        disable_button_theme(g_config_button);
        disable_button_theme(g_account_button);
        disable_button_theme(g_community_button);
        disable_button_theme(g_minimize_button);
        disable_button_theme(g_close_button);

        SetWindowSubclass(g_start_button, button_subclass_proc, 1, 0);
        SetWindowSubclass(g_repair_button, button_subclass_proc, 2, 0);
        SetWindowSubclass(g_config_button, button_subclass_proc, 3, 0);
        SetWindowSubclass(g_account_button, button_subclass_proc, 4, 0);
        SetWindowSubclass(g_community_button, button_subclass_proc, 5, 0);
        SetWindowSubclass(g_minimize_button, button_subclass_proc, 6, 0);
        SetWindowSubclass(g_close_button, button_subclass_proc, 7, 0);

        create_fonts();
        calculate_layout(hwnd);
        move_controls();
        apply_fonts();
        initialize_webview();
    }

    void initialize_ui_state()
    {
        g_progress_value = 0;
        g_status_text = g_config.initial_status_text;
        g_progress_text = g_config.initial_progress_text;

        if (g_start_button)
            EnableWindow(g_start_button, g_config.start_enabled ? TRUE : FALSE);

        if (g_repair_button)
            EnableWindow(g_repair_button, TRUE);
        if (g_config_button)
            EnableWindow(g_config_button, TRUE);
        if (g_account_button)
            EnableWindow(g_account_button, TRUE);
        if (g_community_button)
            EnableWindow(g_community_button, TRUE);
    }

    void draw_background(Graphics &g, HDC hdc)
    {
        RECT client{};
        GetClientRect(g_main_window, &client);
        FillRect(hdc, &client, g_background_brush);

        SolidBrush bg(to_color(g_theme.app_bg_alt, 255));
        g.FillRectangle(&bg, 0, 0, client.right, client.bottom);

        SolidBrush top_glow(Color(255, 16, 18, 24));
        g.FillRectangle(&top_glow, 0, 0, client.right, 140);

        SolidBrush bottom_glow(Color(255, 12, 14, 18));
        g.FillRectangle(&bottom_glow, 0, client.bottom - 140, client.right, 140);
    }

    void draw_topbar(Graphics &g, HDC hdc)
    {
        RECT top = g_layout.topbar;

        SolidBrush brush(to_color(g_theme.topbar_fill, 255));
        g.FillRectangle(&brush, top.left, top.top, top.right - top.left, top.bottom - top.top);

        Pen line(to_color(g_theme.topbar_line, 255), 1.0f);
        g.DrawLine(&line,
                   static_cast<REAL>(g_theme.outer_pad),
                   static_cast<REAL>(top.bottom - 1),
                   static_cast<REAL>(top.right - g_theme.outer_pad),
                   static_cast<REAL>(top.bottom - 1));

        SetBkMode(hdc, TRANSPARENT);

        RECT logo_rect{24, 4, 240, 38};
        SelectObject(hdc, g_logo_font);
        SetTextColor(hdc, g_theme.text);
        DrawTextW(hdc, g_config.logo_text.c_str(), -1, &logo_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        RECT sub_rect{26, 38, 320, 62};
        SelectObject(hdc, g_logo_sub_font);
        SetTextColor(hdc, g_theme.text_accent);
        DrawTextW(hdc, g_config.topbar_subtitle.c_str(), -1, &sub_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    void draw_hero_shell(Graphics &g, HDC hdc)
    {
        fill_rounded_panel(g, g_layout.hero_shell,
                           g_theme.hero_shell, g_theme.hero_shell_line,
                           g_theme.hero_radius, 255, 255, 1.0f);

        fill_rounded_panel(g, g_layout.hero_overlay,
                           g_theme.overlay_fill, g_theme.overlay_line,
                           16, 255, 255, 1.0f);

        SetBkMode(hdc, TRANSPARENT);

        RECT badge{
            g_layout.hero_overlay.left + 18,
            g_layout.hero_overlay.top + 12,
            g_layout.hero_overlay.left + 190,
            g_layout.hero_overlay.top + 28};
        SelectObject(hdc, g_micro_font);
        SetTextColor(hdc, g_theme.text_accent);
        DrawTextW(hdc, L"WEB PREVIEW", -1, &badge, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        RECT title{
            g_layout.hero_overlay.left + 18,
            g_layout.hero_overlay.top + 34,
            g_layout.hero_overlay.right - 18,
            g_layout.hero_overlay.top + 90};
        SelectObject(hdc, g_title_font);
        SetTextColor(hdc, g_theme.text);
        DrawTextW(hdc, g_config.hero_title.c_str(), -1, &title, DT_LEFT | DT_TOP | DT_WORDBREAK);

        RECT subtitle{
            g_layout.hero_overlay.left + 18,
            g_layout.hero_overlay.top + 100,
            g_layout.hero_overlay.right - 18,
            g_layout.hero_overlay.bottom - 16};
        SelectObject(hdc, g_subtitle_font);
        SetTextColor(hdc, g_theme.text_soft);
        DrawTextW(hdc, g_config.hero_subtitle.c_str(), -1, &subtitle, DT_LEFT | DT_TOP | DT_WORDBREAK);
    }

    void draw_side_panel(Graphics &g, HDC hdc)
    {
        fill_rounded_panel(g, g_layout.side_panel,
                           g_theme.side_panel_fill, g_theme.side_panel_line,
                           g_theme.panel_radius, 255, 255, 1.0f);

        SetBkMode(hdc, TRANSPARENT);

        RECT title{
            g_layout.side_panel.left + 14,
            g_layout.side_panel.top + 6,
            g_layout.side_panel.right - 14,
            g_layout.side_panel.top + 20};
        SelectObject(hdc, g_micro_font);
        SetTextColor(hdc, g_theme.text_muted);
        DrawTextW(hdc, L"ACTIONS", -1, &title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    void draw_footer(Graphics &g, HDC hdc)
    {
        fill_rounded_panel(g, g_layout.footer,
                           g_theme.footer_fill, g_theme.footer_line,
                           g_theme.footer_radius, 255, 255, 1.0f);

        SetBkMode(hdc, TRANSPARENT);

        RECT status_label = g_layout.footer_status;
        status_label.bottom = status_label.top + 16;
        SelectObject(hdc, g_micro_font);
        SetTextColor(hdc, g_theme.text_muted);
        DrawTextW(hdc, L"STATUS", -1, &status_label, DT_LEFT | DT_TOP | DT_SINGLELINE);

        RECT status_value = g_layout.footer_status;
        status_value.top += 18;
        SelectObject(hdc, g_footer_font);
        SetTextColor(hdc, g_theme.text);
        DrawTextW(hdc, g_status_text.c_str(), -1, &status_value, DT_LEFT | DT_TOP | DT_SINGLELINE);

        RECT progress_label = g_layout.footer_progress_label;
        SelectObject(hdc, g_micro_font);
        SetTextColor(hdc, g_theme.text_muted);
        DrawTextW(hdc, g_progress_text.c_str(), -1, &progress_label, DT_LEFT | DT_TOP | DT_SINGLELINE);

        fill_rounded_panel(g, g_layout.footer_progress,
                           g_theme.progress_bg, g_theme.progress_line,
                           10, 255, 255, 1.0f);

        const int inner_pad = 3;
        const int full_w = (g_layout.footer_progress.right - g_layout.footer_progress.left) - inner_pad * 2;
        const int used = (full_w * (std::clamp(g_progress_value, 0, 100))) / 100;

        if (used > 0)
        {
            RECT bar{
                g_layout.footer_progress.left + inner_pad,
                g_layout.footer_progress.top + inner_pad,
                g_layout.footer_progress.left + inner_pad + used,
                g_layout.footer_progress.bottom - inner_pad};

            fill_rounded_panel(g, bar,
                               g_theme.progress_fill, g_theme.progress_fill,
                               8, 255, 0, 0.0f);
        }

        wchar_t percent[16]{};
        wsprintfW(percent, L"%d%%", g_progress_value);

        RECT p = g_layout.footer_percent;
        SelectObject(hdc, g_footer_font);
        SetTextColor(hdc, g_theme.text_soft);
        DrawTextW(hdc, percent, -1, &p, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    void draw_button(const DRAWITEMSTRUCT *dis)
    {
        HDC hdc = dis->hDC;
        RECT rc = dis->rcItem;
        UINT state = dis->itemState;
        const int id = static_cast<int>(dis->CtlID);

        const bool pressed = (state & ODS_SELECTED) != 0;
        const bool disabled = (state & ODS_DISABLED) != 0;
        const bool focused = (state & ODS_FOCUS) != 0;

        const bool is_start = (id == IDC_START);
        const bool is_repair = (id == IDC_REPAIR);
        const bool is_close = (id == IDC_CLOSE);
        const bool is_minimize = (id == IDC_MINIMIZE);
        const bool is_top = is_close || is_minimize;

        COLORREF bg = g_theme.secondary_fill;
        COLORREF border = RGB(56, 76, 104);
        COLORREF text = g_theme.text;
        int radius = is_top ? g_theme.top_button_radius : g_theme.button_radius;

        if (is_start)
        {
            bg = pressed ? g_theme.start_pressed : g_theme.start_fill;
            border = RGB(118, 178, 255);
            text = RGB(255, 255, 255);
            radius = g_theme.primary_radius;
        }
        else if (is_repair)
        {
            bg = pressed ? g_theme.repair_pressed : g_theme.repair_fill;
            border = RGB(190, 132, 72);
            text = RGB(255, 248, 240);
            radius = g_theme.primary_radius;
        }
        else if (is_close)
        {
            bg = pressed ? RGB(132, 42, 58) : g_theme.top_button_close;
            border = RGB(186, 98, 114);
            text = RGB(250, 246, 244);
        }
        else if (is_minimize)
        {
            bg = pressed ? RGB(28, 36, 46) : g_theme.top_button_fill;
            border = RGB(90, 106, 130);
            text = RGB(238, 236, 232);
        }
        else
        {
            bg = pressed ? g_theme.secondary_pressed : g_theme.secondary_fill;
            border = RGB(70, 92, 122);
            text = g_theme.text;
        }

        if (disabled)
        {
            bg = RGB(28, 32, 38);
            border = RGB(46, 52, 60);
            text = RGB(110, 118, 128);
        }

        Graphics g(hdc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHalf);
        g.SetCompositingQuality(CompositingQualityHighQuality);

        RECT fill = rc;
        InflateRect(&fill, -1, -1);

        HBRUSH clear_brush = CreateSolidBrush(g_theme.side_panel_fill);
        FillRect(hdc, &rc, clear_brush);
        DeleteObject(clear_brush);

        GraphicsPath path;
        build_round_path(path, fill, radius);

        SolidBrush fill_brush(to_color(bg, 255));
        g.FillPath(&fill_brush, &path);

        Pen border_pen(to_color(border, 255), 1.0f);
        g.DrawPath(&border_pen, &path);

        if (focused && !disabled && !is_top)
        {
            RECT inner = fill;
            InflateRect(&inner, -2, -2);

            GraphicsPath inner_path;
            build_round_path(inner_path, inner, radius > 4 ? radius - 2 : radius);

            Pen focus_pen(to_color(RGB(98, 154, 255), 80), 1.0f);
            g.DrawPath(&focus_pen, &inner_path);
        }

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, text);

        SelectObject(
            hdc,
            is_top ? g_top_button_font : ((is_start || is_repair) ? g_button_large_font : g_button_font));

        const wchar_t *label =
            (id == IDC_START)       ? L"PLAY"
            : (id == IDC_REPAIR)    ? L"REPAIR"
            : (id == IDC_CONFIG)    ? L"SETTINGS"
            : (id == IDC_ACCOUNT)   ? L"ACCOUNT"
            : (id == IDC_COMMUNITY) ? L"COMMUNITY"
            : (id == IDC_MINIMIZE)  ? L"_"
                                    : L"X";

        RECT tr = fill;
        DrawTextW(hdc, label, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    void open_url(HWND hwnd, const std::wstring &url)
    {
        if (url.empty())
        {
            MessageBoxW(hwnd, L"URL is not configured.", L"Launcher", MB_OK | MB_ICONWARNING);
            return;
        }

        HINSTANCE result = ShellExecuteW(hwnd, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(result) <= 32)
            MessageBoxW(hwnd, L"Failed to open web page.", L"Launcher", MB_OK | MB_ICONERROR);
    }

    void launch_config(HWND hwnd)
    {
        if (g_config.config_path.empty() || !file_exists(g_config.config_path))
        {
            MessageBoxW(hwnd, L"Config executable not found.", L"Launcher", MB_OK | MB_ICONERROR);
            return;
        }

        HINSTANCE result = ShellExecuteW(hwnd, L"open", g_config.config_path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(result) <= 32)
            MessageBoxW(hwnd, L"Failed to launch config executable.", L"Launcher", MB_OK | MB_ICONERROR);
    }

    LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        switch (msg)
        {
        case WM_CREATE:
            g_main_window = hwnd;
            g_background_brush = CreateSolidBrush(g_theme.app_bg);
            g_preview_brush = CreateSolidBrush(RGB(14, 18, 24));

            load_launcher_config();
            load_assets();
            create_controls(hwnd);
            initialize_ui_state();
            apply_window_region(hwnd);
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_SIZE:
        {
            if (wparam == SIZE_MINIMIZED)
                return 0;

            calculate_layout(hwnd);
            move_controls();
            apply_window_region(hwnd);

            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_SHOWWINDOW:
        {
            if (wparam)
            {
                calculate_layout(hwnd);
                move_controls();
                apply_window_region(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_CTLCOLORSTATIC:
        {
            HDC hdc = reinterpret_cast<HDC>(wparam);
            HWND control = reinterpret_cast<HWND>(lparam);

            if (control == g_preview_host)
            {
                SetBkColor(hdc, RGB(14, 18, 24));
                SetTextColor(hdc, RGB(14, 18, 24));
                return reinterpret_cast<INT_PTR>(g_preview_brush);
            }

            break;
        }

            // case WM_CTLCOLORBTN:
            // {
            //     HDC hdc = reinterpret_cast<HDC>(wparam);
            //     SetBkMode(hdc, TRANSPARENT);
            //     return reinterpret_cast<INT_PTR>(GetStockObject(NULL_BRUSH));
            // }

        case WM_NCHITTEST:
        {
            LRESULT hit = DefWindowProcW(hwnd, msg, wparam, lparam);

            if (hit == HTCLIENT)
            {
                POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                ScreenToClient(hwnd, &pt);

                if (PtInRect(&g_layout.btn_minimize, pt) || PtInRect(&g_layout.btn_close, pt))
                    return HTCLIENT;

                if (pt.y < g_theme.topbar_h)
                    return HTCAPTION;
            }

            return hit;
        }

        case WM_DRAWITEM:
            draw_button(reinterpret_cast<DRAWITEMSTRUCT *>(lparam));
            return TRUE;

        case WM_COMMAND:
        {
            const int id = LOWORD(wparam);

            switch (id)
            {
            case IDC_CLOSE:
                DestroyWindow(hwnd);
                return 0;

            case IDC_MINIMIZE:
                ShowWindow(hwnd, SW_MINIMIZE);
                return 0;

            case IDC_START:
                launcher::start_patch_flow();
                return 0;

            case IDC_REPAIR:
                launcher::start_repair_flow();
                return 0;

            case IDC_CONFIG:
                launch_config(hwnd);
                return 0;

            case IDC_ACCOUNT:
                open_url(hwnd, g_config.account_url);
                return 0;

            case IDC_COMMUNITY:
                open_url(hwnd, g_config.community_url);
                return 0;
            }

            break;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT client{};
            GetClientRect(hwnd, &client);

            HDC memdc = CreateCompatibleDC(hdc);
            HBITMAP membmp = CreateCompatibleBitmap(hdc, client.right, client.bottom);
            HBITMAP oldbmp = static_cast<HBITMAP>(SelectObject(memdc, membmp));

            FillRect(memdc, &client, g_background_brush);

            Graphics g(memdc);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetPixelOffsetMode(PixelOffsetModeHalf);
            g.SetCompositingQuality(CompositingQualityHighQuality);

            draw_background(g, memdc);
            draw_topbar(g, memdc);
            draw_hero_shell(g, memdc);
            draw_side_panel(g, memdc);
            draw_footer(g, memdc);

            BitBlt(hdc, 0, 0, client.right, client.bottom, memdc, 0, 0, SRCCOPY);

            SelectObject(memdc, oldbmp);
            DeleteObject(membmp);
            DeleteDC(memdc);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            g_webview.Reset();
            g_webview_controller.Reset();
            g_webview_ready = false;
            g_webview_initializing = false;

            destroy_fonts();

            if (g_preview_brush)
                DeleteObject(g_preview_brush);
            g_preview_brush = nullptr;

            if (g_background_brush)
                DeleteObject(g_background_brush);
            g_background_brush = nullptr;

            g_background_image.reset();
            shutdown_gdiplus();

            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

namespace ui
{
    bool register_window_class(HINSTANCE instance)
    {
        HICON big_icon = LoadIconW(instance, MAKEINTRESOURCEW(kAppIconId));
        HICON small_icon = LoadIconW(instance, MAKEINTRESOURCEW(kAppIconId));

        load_launcher_config();

        if (!big_icon && file_exists(g_config.icon_path))
        {
            big_icon = static_cast<HICON>(
                LoadImageW(nullptr, g_config.icon_path.c_str(), IMAGE_ICON, 64, 64, LR_LOADFROMFILE));
        }

        if (!small_icon && file_exists(g_config.icon_path))
        {
            small_icon = static_cast<HICON>(
                LoadImageW(nullptr, g_config.icon_path.c_str(), IMAGE_ICON, 32, 32, LR_LOADFROMFILE));
        }

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc = wnd_proc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = kWindowClassName;
        wc.hIcon = big_icon;
        wc.hIconSm = small_icon;

        return RegisterClassExW(&wc) != 0;
    }

    HWND create_main_window(HINSTANCE instance, int show_cmd)
    {
        const wchar_t *title = g_config.window_title.empty() ? L"Launcher" : g_config.window_title.c_str();

        HWND hwnd = CreateWindowExW(
            WS_EX_APPWINDOW,
            kWindowClassName,
            title,
            WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            90,
            28,
            g_theme.window_width,
            g_theme.window_height,
            nullptr,
            nullptr,
            instance,
            nullptr);

        if (!hwnd)
            return nullptr;

        HICON big_icon = LoadIconW(instance, MAKEINTRESOURCEW(kAppIconId));
        HICON small_icon = LoadIconW(instance, MAKEINTRESOURCEW(kAppIconId));

        if (!big_icon && file_exists(g_config.icon_path))
        {
            big_icon = static_cast<HICON>(
                LoadImageW(nullptr, g_config.icon_path.c_str(), IMAGE_ICON, 64, 64, LR_LOADFROMFILE));
        }

        if (!small_icon && file_exists(g_config.icon_path))
        {
            small_icon = static_cast<HICON>(
                LoadImageW(nullptr, g_config.icon_path.c_str(), IMAGE_ICON, 32, 32, LR_LOADFROMFILE));
        }

        if (big_icon)
            SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(big_icon));
        if (small_icon)
            SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon));

        ShowWindow(hwnd, show_cmd);
        UpdateWindow(hwnd);
        return hwnd;
    }

    void set_status_text(const std::wstring &text)
    {
        g_status_text = text;
        if (g_main_window)
            InvalidateRect(g_main_window, &g_layout.footer, FALSE);
    }

    void set_progress_text(const std::wstring &text)
    {
        g_progress_text = text;
        if (g_main_window)
            InvalidateRect(g_main_window, &g_layout.footer, FALSE);
    }

    void set_progress_value(int value)
    {
        g_progress_value = std::clamp(value, 0, 100);
        if (g_main_window)
            InvalidateRect(g_main_window, &g_layout.footer, FALSE);
    }

    void set_start_enabled(bool enabled)
    {
        if (g_start_button)
            EnableWindow(g_start_button, enabled ? TRUE : FALSE);
    }

    void set_controls_enabled(bool enabled)
    {
        if (g_start_button)
            EnableWindow(g_start_button, enabled ? TRUE : FALSE);
        if (g_repair_button)
            EnableWindow(g_repair_button, enabled ? TRUE : FALSE);
        if (g_config_button)
            EnableWindow(g_config_button, enabled ? TRUE : FALSE);
        if (g_account_button)
            EnableWindow(g_account_button, enabled ? TRUE : FALSE);
        if (g_community_button)
            EnableWindow(g_community_button, enabled ? TRUE : FALSE);
    }

    void set_hero_title(const std::wstring &text)
    {
        g_config.hero_title = text;
        if (g_main_window)
            InvalidateRect(g_main_window, &g_layout.hero_overlay, FALSE);
    }

    void set_hero_subtitle(const std::wstring &text)
    {
        g_config.hero_subtitle = text;
        if (g_main_window)
            InvalidateRect(g_main_window, &g_layout.hero_overlay, FALSE);
    }

    std::wstring game_path()
    {
        return g_config.game_path;
    }

    std::wstring config_path()
    {
        return g_config.config_path;
    }

    std::wstring preview_url()
    {
        return g_config.preview_url;
    }

    HWND main_window()
    {
        return g_main_window;
    }
}