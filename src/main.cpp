#include "../include/ui.hpp"

#include <windows.h>
#include <commctrl.h>
#include <objbase.h>

#pragma comment(lib, "comctl32.lib")

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_cmd)
{
    HRESULT com_hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    if (!ui::register_window_class(instance))
    {
        MessageBoxW(
            nullptr,
            L"Failed to register window class.",
            L"Launcher",
            MB_OK | MB_ICONERROR);

        if (SUCCEEDED(com_hr))
            CoUninitialize();

        return 1;
    }

    HWND hwnd = ui::create_main_window(instance, show_cmd);
    if (!hwnd)
    {
        MessageBoxW(
            nullptr,
            L"Failed to create main window.",
            L"Launcher",
            MB_OK | MB_ICONERROR);

        if (SUCCEEDED(com_hr))
            CoUninitialize();

        return 1;
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (SUCCEEDED(com_hr))
        CoUninitialize();

    return static_cast<int>(msg.wParam);
}