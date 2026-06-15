#include "ui/settings_window.h"
#include <CommCtrl.h>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <utility>

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
#define IDC_COMBO_LANGUAGE  2031
#define IDC_COMBO_INTERVAL  2032
#define IDC_BTN_APPLY       2040
#define IDC_BTN_CANCEL      2041
#define IDC_INFO_TEXT       2050
#define IDC_GROUP_OVERLAY   2060
#define IDC_GROUP_METRICS   2061
#define IDC_GROUP_APPEAR    2062
#define IDC_GROUP_DATA      2063
#define IDC_LABEL_THEME     2070
#define IDC_LABEL_LANGUAGE  2071
#define IDC_LABEL_INTERVAL  2072

static constexpr UINT_PTR INFO_TIMER_ID = 10;

static const int UPDATE_INTERVALS[] = { 66, 250, 500, 1000, 2000 };

SettingsWindow::SettingsWindow() = default;
SettingsWindow::~SettingsWindow() = default;

bool SettingsWindow::Show(HWND parentHwnd, Config& config, ReadingsProvider readingsProvider)
{
    config_ = &config;
    tempConfig_ = config; // Make a working copy
    readingsProvider_ = std::move(readingsProvider);

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

    case WM_TIMER:
        OnTimer(hwnd, wParam);
        return TRUE;

    case WM_COMMAND:
        OnCommand(hwnd, wParam);
        return TRUE;

    case WM_CLOSE:
        OnCancel(hwnd);
        EndDialog(hwnd, IDCANCEL);
        return TRUE;
    }

    return FALSE;
}

void SettingsWindow::OnInitDialog(HWND hwnd)
{
    PopulateCombos(hwnd);
    ApplyLanguage(hwnd);

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

    UpdateInfo(hwnd);
    SetTimer(hwnd, INFO_TIMER_ID,
             static_cast<UINT>(tempConfig_.overlay.updateIntervalMs), nullptr);
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

    HWND hLangCombo = GetDlgItem(hwnd, IDC_COMBO_LANGUAGE);
    if (hLangCombo) {
        int sel = static_cast<int>(SendMessage(hLangCombo, CB_GETCURSEL, 0, 0));
        tempConfig_.general.language = (sel == 1) ? "en" : "zh";
    }

    tempConfig_.overlay.updateIntervalMs = SelectedUpdateIntervalMs(hwnd);

    // Apply changes
    *config_ = tempConfig_;
    config_->Save();
}

void SettingsWindow::OnCancel(HWND /*hwnd*/)
{
    // Discard changes (tempConfig_ is not applied)
}

void SettingsWindow::OnTimer(HWND hwnd, WPARAM timerId)
{
    if (timerId == INFO_TIMER_ID) {
        UpdateInfo(hwnd);
    }
}

void SettingsWindow::OnCommand(HWND hwnd, WPARAM wParam)
{
    const auto controlId = LOWORD(wParam);
    const auto notification = HIWORD(wParam);

    switch (controlId) {
    case IDC_BTN_APPLY:
        KillTimer(hwnd, INFO_TIMER_ID);
        OnApply(hwnd);
        EndDialog(hwnd, IDOK);
        return;

    case IDC_BTN_CANCEL:
    case IDCANCEL:
        KillTimer(hwnd, INFO_TIMER_ID);
        OnCancel(hwnd);
        EndDialog(hwnd, IDCANCEL);
        return;

    case IDC_COMBO_LANGUAGE:
        if (notification == CBN_SELCHANGE) {
            HWND hLangCombo = GetDlgItem(hwnd, IDC_COMBO_LANGUAGE);
            int sel = static_cast<int>(SendMessage(hLangCombo, CB_GETCURSEL, 0, 0));
            tempConfig_.general.language = (sel == 1) ? "en" : "zh";
            ApplyLanguage(hwnd);
            UpdateInfo(hwnd);
        }
        return;

    case IDC_COMBO_INTERVAL:
        if (notification == CBN_SELCHANGE) {
            tempConfig_.overlay.updateIntervalMs = SelectedUpdateIntervalMs(hwnd);
            KillTimer(hwnd, INFO_TIMER_ID);
            SetTimer(hwnd, INFO_TIMER_ID,
                     static_cast<UINT>(tempConfig_.overlay.updateIntervalMs), nullptr);
            UpdateInfo(hwnd);
        }
        return;
    }
}

void SettingsWindow::ApplyLanguage(HWND hwnd)
{
    SetWindowText(hwnd, Text(L"\u6027\u80FD\u76D1\u63A7\u8BBE\u7F6E", L"Performance Monitor Settings").c_str());
    SetDlgItemText(hwnd, IDC_GROUP_OVERLAY, Text(L"\u663E\u793A\u6A21\u5F0F", L"Overlay Mode").c_str());
    SetDlgItemText(hwnd, IDC_RADIO_HUD, Text(L"HUD\uFF08\u5934\u663E\u56FA\u5B9A\uFF09", L"HUD (Head-locked)").c_str());
    SetDlgItemText(hwnd, IDC_RADIO_WRIST, Text(L"\u624B\u8155\uFF08\u63A7\u5236\u5668\uFF09", L"Wrist (Controller)").c_str());
    SetDlgItemText(hwnd, IDC_GROUP_METRICS, Text(L"\u76D1\u63A7\u9879", L"Metrics").c_str());
    SetDlgItemText(hwnd, IDC_CHECK_CPU_LOAD, Text(L"CPU \u8D1F\u8F7D", L"CPU Load").c_str());
    SetDlgItemText(hwnd, IDC_CHECK_CPU_TEMP, Text(L"CPU \u6E29\u5EA6", L"CPU Temp").c_str());
    SetDlgItemText(hwnd, IDC_CHECK_GPU_LOAD, Text(L"GPU \u8D1F\u8F7D", L"GPU Load").c_str());
    SetDlgItemText(hwnd, IDC_CHECK_GPU_TEMP, Text(L"GPU \u6E29\u5EA6", L"GPU Temp").c_str());
    SetDlgItemText(hwnd, IDC_CHECK_GPU_MEM, Text(L"\u663E\u5B58", L"VRAM").c_str());
    SetDlgItemText(hwnd, IDC_CHECK_RAM, Text(L"\u5185\u5B58", L"RAM").c_str());
    SetDlgItemText(hwnd, IDC_GROUP_APPEAR, Text(L"\u754C\u9762", L"Appearance").c_str());
    SetDlgItemText(hwnd, IDC_LABEL_THEME, Text(L"\u4E3B\u9898\uFF1A", L"Theme:").c_str());
    SetDlgItemText(hwnd, IDC_LABEL_LANGUAGE, Text(L"\u8BED\u8A00\uFF1A", L"Language:").c_str());
    SetDlgItemText(hwnd, IDC_LABEL_INTERVAL, Text(L"\u66F4\u65B0\u9891\u7387\uFF1A", L"Update:").c_str());
    SetDlgItemText(hwnd, IDC_GROUP_DATA, Text(L"\u5F53\u524D\u6570\u636E", L"Live Data").c_str());
    SetDlgItemText(hwnd, IDC_BTN_APPLY, Text(L"\u5E94\u7528", L"Apply").c_str());
    SetDlgItemText(hwnd, IDC_BTN_CANCEL, Text(L"\u53D6\u6D88", L"Cancel").c_str());
}

void SettingsWindow::PopulateCombos(HWND hwnd)
{
    HWND hThemeCombo = GetDlgItem(hwnd, IDC_COMBO_THEME);
    if (hThemeCombo) {
        SendMessage(hThemeCombo, CB_RESETCONTENT, 0, 0);
        SendMessage(hThemeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Dark"));
        SendMessage(hThemeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Light"));
        SendMessage(hThemeCombo, CB_SETCURSEL,
                    tempConfig_.appearance.theme == "dark" ? 0 : 1, 0);
    }

    HWND hLangCombo = GetDlgItem(hwnd, IDC_COMBO_LANGUAGE);
    if (hLangCombo) {
        SendMessage(hLangCombo, CB_RESETCONTENT, 0, 0);
        SendMessage(hLangCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"\u4E2D\u6587"));
        SendMessage(hLangCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"English"));
        SendMessage(hLangCombo, CB_SETCURSEL,
                    tempConfig_.general.language == "en" ? 1 : 0, 0);
    }

    HWND hIntervalCombo = GetDlgItem(hwnd, IDC_COMBO_INTERVAL);
    if (hIntervalCombo) {
        SendMessage(hIntervalCombo, CB_RESETCONTENT, 0, 0);
        for (int interval : UPDATE_INTERVALS) {
            std::wstringstream label;
            label << interval << L" ms";
            SendMessage(hIntervalCombo, CB_ADDSTRING, 0,
                        reinterpret_cast<LPARAM>(label.str().c_str()));
        }
        SelectUpdateInterval(hwnd, tempConfig_.overlay.updateIntervalMs);
    }
}

void SettingsWindow::UpdateInfo(HWND hwnd)
{
    std::wstringstream info;
    const auto readings = readingsProvider_ ? readingsProvider_() : std::vector<SensorReading>{};

    if (readings.empty()) {
        info << Text(L"\u6682\u65E0\u6570\u636E\u3002\r\n", L"No sensor data available.\r\n");
        info << Text(L"\u8BF7\u786E\u8BA4 LibreHardwareMonitor bridge \u6B63\u5728\u8FD0\u884C\u3002",
                     L"Confirm that the LibreHardwareMonitor bridge is running.");
    } else {
        for (const auto& reading : readings) {
            info << CategoryName(reading.category) << L"  "
                 << std::wstring(reading.label.begin(), reading.label.end())
                 << L": " << FormatReading(reading) << L"\r\n";
        }
    }

    SetDlgItemText(hwnd, IDC_INFO_TEXT, info.str().c_str());
}

std::wstring SettingsWindow::Text(const wchar_t* zh, const wchar_t* en) const
{
    return tempConfig_.general.language == "en" ? en : zh;
}

std::wstring SettingsWindow::FormatReading(const SensorReading& reading) const
{
    std::wstringstream ss;
    switch (reading.category) {
    case SensorCategory::CpuTemp:
    case SensorCategory::GpuTemp:
        ss << std::fixed << std::setprecision(0) << reading.value << L"\u00B0C";
        break;
    case SensorCategory::CpuLoad:
    case SensorCategory::GpuLoad:
    case SensorCategory::RamUsage:
        ss << std::fixed << std::setprecision(0) << reading.value << L"%";
        break;
    case SensorCategory::CpuClock:
    case SensorCategory::GpuClock:
        ss << std::fixed << std::setprecision(0) << reading.value << L" MHz";
        break;
    case SensorCategory::GpuMemory:
        if (reading.value > 1024.0) {
            ss << std::fixed << std::setprecision(1) << (reading.value / 1024.0) << L" GB";
        } else {
            ss << std::fixed << std::setprecision(0) << reading.value << L" MB";
        }
        break;
    case SensorCategory::GpuFan:
    case SensorCategory::Fan:
        ss << std::fixed << std::setprecision(0) << reading.value << L" RPM";
        break;
    case SensorCategory::Power:
        ss << std::fixed << std::setprecision(1) << reading.value << L" W";
        break;
    case SensorCategory::Voltage:
        ss << std::fixed << std::setprecision(2) << reading.value << L" V";
        break;
    default:
        ss << std::fixed << std::setprecision(1) << reading.value;
        if (!reading.unit.empty()) {
            ss << L" " << std::wstring(reading.unit.begin(), reading.unit.end());
        }
        break;
    }

    return ss.str();
}

std::wstring SettingsWindow::CategoryName(SensorCategory category) const
{
    switch (category) {
    case SensorCategory::CpuLoad: return Text(L"CPU \u8D1F\u8F7D", L"CPU Load");
    case SensorCategory::CpuTemp: return Text(L"CPU \u6E29\u5EA6", L"CPU Temp");
    case SensorCategory::CpuClock: return Text(L"CPU \u9891\u7387", L"CPU Clock");
    case SensorCategory::GpuLoad: return Text(L"GPU \u8D1F\u8F7D", L"GPU Load");
    case SensorCategory::GpuTemp: return Text(L"GPU \u6E29\u5EA6", L"GPU Temp");
    case SensorCategory::GpuClock: return Text(L"GPU \u9891\u7387", L"GPU Clock");
    case SensorCategory::GpuMemory: return Text(L"\u663E\u5B58", L"VRAM");
    case SensorCategory::GpuFan: return Text(L"GPU \u98CE\u6247", L"GPU Fan");
    case SensorCategory::RamUsage: return Text(L"\u5185\u5B58", L"RAM");
    case SensorCategory::Fan: return Text(L"\u98CE\u6247", L"Fan");
    case SensorCategory::Voltage: return Text(L"\u7535\u538B", L"Voltage");
    case SensorCategory::Power: return Text(L"\u529F\u8017", L"Power");
    default: return Text(L"\u672A\u77E5", L"Unknown");
    }
}

int SettingsWindow::SelectedUpdateIntervalMs(HWND hwnd) const
{
    HWND hIntervalCombo = GetDlgItem(hwnd, IDC_COMBO_INTERVAL);
    if (!hIntervalCombo) {
        return tempConfig_.overlay.updateIntervalMs;
    }

    const int sel = static_cast<int>(SendMessage(hIntervalCombo, CB_GETCURSEL, 0, 0));
    if (sel >= 0 && sel < static_cast<int>(std::size(UPDATE_INTERVALS))) {
        return UPDATE_INTERVALS[sel];
    }

    return tempConfig_.overlay.updateIntervalMs;
}

void SettingsWindow::SelectUpdateInterval(HWND hwnd, int intervalMs)
{
    HWND hIntervalCombo = GetDlgItem(hwnd, IDC_COMBO_INTERVAL);
    if (!hIntervalCombo) {
        return;
    }

    int selected = 0;
    for (int i = 0; i < static_cast<int>(std::size(UPDATE_INTERVALS)); ++i) {
        if (UPDATE_INTERVALS[i] == intervalMs) {
            selected = i;
            break;
        }
    }

    SendMessage(hIntervalCombo, CB_SETCURSEL, selected, 0);
}

} // namespace vrperf
