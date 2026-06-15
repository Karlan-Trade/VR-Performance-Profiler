#pragma once

#include <Windows.h>
#include <shellapi.h>
#include <string>

namespace vrperf {

enum TrayMenuID {
    TRAY_MENU_TOGGLE_VIS = 1001,
    TRAY_MENU_SWITCH_MODE,
    TRAY_MENU_SETTINGS,
    TRAY_MENU_EXIT
};

class TrayIcon {
public:
    TrayIcon();
    ~TrayIcon();

    bool Create(HWND hwnd, HINSTANCE hInstance, UINT callbackMsg);
    void Destroy();

    void UpdateTooltip(const std::wstring& text);
    void ShowMenu(HWND hwnd, int x, int y);

    bool IsCreated() const { return created_; }

private:
    NOTIFYICONDATA nid_ = {};
    bool created_ = false;
};

} // namespace vrperf
