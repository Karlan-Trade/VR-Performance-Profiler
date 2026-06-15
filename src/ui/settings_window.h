#pragma once

#include "core/config.h"
#include <Windows.h>

namespace vrperf {

class SettingsWindow {
public:
    SettingsWindow();
    ~SettingsWindow();

    // Show the settings dialog (modal)
    bool Show(HWND parentHwnd, Config& config);

private:
    static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    INT_PTR HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void OnInitDialog(HWND hwnd);
    void OnApply(HWND hwnd);
    void OnCancel(HWND hwnd);

    Config* config_ = nullptr;
    Config tempConfig_;  // Working copy for editing
};

} // namespace vrperf
