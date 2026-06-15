#include "ui/tray_icon.h"
#include <strsafe.h>

namespace vrperf {

TrayIcon::TrayIcon() = default;

TrayIcon::~TrayIcon()
{
    Destroy();
}

bool TrayIcon::Create(HWND hwnd, HINSTANCE hInstance, UINT callbackMsg)
{
    if (created_) return true;

    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = hwnd;
    nid_.uID = 1;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = callbackMsg;

    // Try to load icon from resources, fallback to default
    nid_.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(101));
    if (!nid_.hIcon) {
        nid_.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    }

    StringCchCopy(nid_.szTip, ARRAYSIZE(nid_.szTip), L"VR Performance Profiler");

    if (!Shell_NotifyIcon(NIM_ADD, &nid_)) {
        return false;
    }

    created_ = true;
    return true;
}

void TrayIcon::Destroy()
{
    if (created_) {
        Shell_NotifyIcon(NIM_DELETE, &nid_);
        created_ = false;
    }
}

void TrayIcon::UpdateTooltip(const std::wstring& text)
{
    if (!created_) return;

    // Truncate if too long
    StringCchCopy(nid_.szTip, ARRAYSIZE(nid_.szTip), text.c_str());
    Shell_NotifyIcon(NIM_MODIFY, &nid_);
}

void TrayIcon::ShowMenu(HWND hwnd, int x, int y, const std::string& language)
{
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    const bool english = language == "en";

    AppendMenu(hMenu, MF_STRING, TRAY_MENU_CONNECT_VR,
               english ? L"Connect SteamVR Overlay" : L"\u8FDE\u63A5 SteamVR \u8986\u76D6");
    AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hMenu, MF_STRING, TRAY_MENU_TOGGLE_VIS,
               english ? L"Toggle Overlay Visibility" : L"\u5207\u6362\u8986\u76D6\u663E\u793A");
    AppendMenu(hMenu, MF_STRING, TRAY_MENU_SWITCH_MODE,
               english ? L"Switch Mode (HUD/Wrist)" : L"\u5207\u6362\u6A21\u5F0F\uFF08HUD/\u624B\u8155\uFF09");
    AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hMenu, MF_STRING, TRAY_MENU_SETTINGS,
               english ? L"Settings..." : L"\u8BBE\u7F6E...");
    AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hMenu, MF_STRING, TRAY_MENU_EXIT,
               english ? L"Exit" : L"\u9000\u51FA");

    // Required to make menu disappear when clicking outside
    SetForegroundWindow(hwnd);

    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, x, y, 0, hwnd, nullptr);

    PostMessage(hwnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

} // namespace vrperf
