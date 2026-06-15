#include "ui/settings_window.h"
#include <CommCtrl.h>

#pragma comment(lib, "comctl32.lib")

namespace vrperf {

// Dialog resource ID (must match .rc file)
#define IDD_SETTINGS 2001

// Control IDs
#define IDC_RADIO_HUD       2010
#define IDC_RADIO_WRIST     2011
#define IDC_CHECK_CPU_LOAD  2020
#define IDC_CHECK_CPU_TEMP  2021
#define IDC_CHECK_GPU_LOAD  2022
#define IDC_CHECK_GPU_TEMP  2023
#define IDC_CHECK_GPU_MEM   2024
#define IDC_CHECK_RAM       2025
#define IDC_COMBO_THEME     2030
#define IDC_BTN_APPLY       2040
#define IDC_BTN_CANCEL      2041

SettingsWindow::SettingsWindow() = default;
SettingsWindow::~SettingsWindow() = default;

bool SettingsWindow::Show(HWND parentHwnd, Config& config)
{
    config_ = &config;
    tempConfig_ = config; // Make a working copy

    INT_PTR result = DialogBoxParam(
        GetModuleHandle(nullptr),
        MAKEINTRESOURCE(IDD_SETTINGS),
        parentHwnd,
        DialogProc,
        reinterpret_cast<LPARAM>(this)
    );

    return result == IDOK;
}

INT_PTR CALLBACK SettingsWindow::DialogProc(HWND hwnd, UINT msg,
                                            WPARAM wParam, LPARAM lParam)
{
    SettingsWindow* self = nullptr;

    if (msg == WM_INITDIALOG) {
        self = reinterpret_cast<SettingsWindow*>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->HandleMessage(hwnd, msg, wParam, lParam);
    }

    return FALSE;
}

INT_PTR SettingsWindow::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_INITDIALOG:
        OnInitDialog(hwnd);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BTN_APPLY:
            OnApply(hwnd);
            EndDialog(hwnd, IDOK);
            return TRUE;

        case IDC_BTN_CANCEL:
        case IDCANCEL:
            OnCancel(hwnd);
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
        }
        break;

    case WM_CLOSE:
        OnCancel(hwnd);
        EndDialog(hwnd, IDCANCEL);
        return TRUE;
    }

    return FALSE;
}

void SettingsWindow::OnInitDialog(HWND hwnd)
{
    // Set initial radio button state
    if (tempConfig_.overlay.mode == "hud") {
        CheckRadioButton(hwnd, IDC_RADIO_HUD, IDC_RADIO_WRIST, IDC_RADIO_HUD);
    } else {
        CheckRadioButton(hwnd, IDC_RADIO_HUD, IDC_RADIO_WRIST, IDC_RADIO_WRIST);
    }

    // Set checkbox states for metrics
    for (const auto& metric : tempConfig_.metrics) {
        int controlId = 0;
        if (metric.category == "cpu_load") controlId = IDC_CHECK_CPU_LOAD;
        else if (metric.category == "cpu_temp") controlId = IDC_CHECK_CPU_TEMP;
        else if (metric.category == "gpu_load") controlId = IDC_CHECK_GPU_LOAD;
        else if (metric.category == "gpu_temp") controlId = IDC_CHECK_GPU_TEMP;
        else if (metric.category == "gpu_memory") controlId = IDC_CHECK_GPU_MEM;
        else if (metric.category == "ram_usage") controlId = IDC_CHECK_RAM;

        if (controlId) {
            CheckDlgButton(hwnd, controlId, metric.enabled ? BST_CHECKED : BST_UNCHECKED);
        }
    }

    // Set theme combo box
    HWND hCombo = GetDlgItem(hwnd, IDC_COMBO_THEME);
    if (hCombo) {
        SendMessage(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Dark"));
        SendMessage(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Light"));
        SendMessage(hCombo, CB_SETCURSEL,
                    tempConfig_.appearance.theme == "dark" ? 0 : 1, 0);
    }
}

void SettingsWindow::OnApply(HWND hwnd)
{
    // Read radio button state
    if (IsDlgButtonChecked(hwnd, IDC_RADIO_HUD) == BST_CHECKED) {
        tempConfig_.overlay.mode = "hud";
    } else {
        tempConfig_.overlay.mode = "wrist";
    }

    // Read checkbox states
    for (auto& metric : tempConfig_.metrics) {
        int controlId = 0;
        if (metric.category == "cpu_load") controlId = IDC_CHECK_CPU_LOAD;
        else if (metric.category == "cpu_temp") controlId = IDC_CHECK_CPU_TEMP;
        else if (metric.category == "gpu_load") controlId = IDC_CHECK_GPU_LOAD;
        else if (metric.category == "gpu_temp") controlId = IDC_CHECK_GPU_TEMP;
        else if (metric.category == "gpu_memory") controlId = IDC_CHECK_GPU_MEM;
        else if (metric.category == "ram_usage") controlId = IDC_CHECK_RAM;

        if (controlId) {
            metric.enabled = (IsDlgButtonChecked(hwnd, controlId) == BST_CHECKED);
        }
    }

    // Read theme
    HWND hCombo = GetDlgItem(hwnd, IDC_COMBO_THEME);
    if (hCombo) {
        int sel = static_cast<int>(SendMessage(hCombo, CB_GETCURSEL, 0, 0));
        tempConfig_.appearance.theme = (sel == 0) ? "dark" : "light";
    }

    // Apply changes
    *config_ = tempConfig_;
    config_->Save();
}

void SettingsWindow::OnCancel(HWND /*hwnd*/)
{
    // Discard changes (tempConfig_ is not applied)
}

} // namespace vrperf
