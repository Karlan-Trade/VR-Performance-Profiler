#include "ui/settings_window.h"
#include <CommCtrl.h>
#include <algorithm>
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
#define IDC_LIST_METRICS    2080
#define IDC_BTN_CONNECT_VR  2090

static constexpr UINT_PTR INFO_TIMER_ID = 10;

static const int UPDATE_INTERVALS[] = { 66, 250, 500, 1000, 2000 };

SettingsWindow::SettingsWindow() = default;
SettingsWindow::~SettingsWindow() = default;

bool SettingsWindow::Show(HWND parentHwnd,
                          Config& config,
                          ReadingsProvider readingsProvider,
                          ApplyCallback applyCallback,
                          ConnectCallback connectCallback)
{
    config_ = &config;
    tempConfig_ = config; // Make a working copy
    readingsProvider_ = std::move(readingsProvider);
    applyCallback_ = std::move(applyCallback);
    connectCallback_ = std::move(connectCallback);
    latestReadings_.clear();

    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);

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
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);

    PopulateCombos(hwnd);
    SetupReadingsList(hwnd);
    ApplyLanguage(hwnd);

    // Set initial radio button state
    if (tempConfig_.overlay.mode == "hud") {
        CheckRadioButton(hwnd, IDC_RADIO_HUD, IDC_RADIO_WRIST, IDC_RADIO_HUD);
    } else {
        CheckRadioButton(hwnd, IDC_RADIO_HUD, IDC_RADIO_WRIST, IDC_RADIO_WRIST);
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

    ReadMetricSelectionsFromList(hwnd);

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
    if (applyCallback_) {
        applyCallback_();
    }
}

void SettingsWindow::OnCancel(HWND /*hwnd*/)
{
    // Discard changes (tempConfig_ is not applied)
}

void SettingsWindow::OnConnectSteamVr(HWND hwnd)
{
    OnApply(hwnd);

    const bool connected = connectCallback_ && connectCallback_();
    MessageBoxW(
        hwnd,
        connected
            ? Text(L"SteamVR \u8986\u76D6\u8FDE\u63A5\u6210\u529F.", L"SteamVR overlay connected.").c_str()
            : Text(L"SteamVR \u8986\u76D6\u8FDE\u63A5\u5931\u8D25.\u8BF7\u5148\u786E\u8BA4 SteamVR \u5DF2\u7ECF\u542F\u52A8,\u7136\u540E\u91CD\u8BD5.",
                   L"SteamVR overlay connection failed. Start SteamVR first, then retry.").c_str(),
        Text(L"SteamVR \u8FDE\u63A5", L"SteamVR Connection").c_str(),
        connected ? MB_OK | MB_ICONINFORMATION : MB_OK | MB_ICONWARNING);
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
        OnApply(hwnd);
        return;

    case IDC_BTN_CONNECT_VR:
        OnConnectSteamVr(hwnd);
        return;

    case IDC_BTN_CANCEL:
    case IDCANCEL:
        KillTimer(hwnd, INFO_TIMER_ID);
        OnCancel(hwnd);
        EndDialog(hwnd, IDCANCEL);
        return;

    case IDC_COMBO_LANGUAGE:
        if (notification == CBN_SELCHANGE) {
            HWND hThemeCombo = GetDlgItem(hwnd, IDC_COMBO_THEME);
            if (hThemeCombo) {
                int themeSel = static_cast<int>(SendMessage(hThemeCombo, CB_GETCURSEL, 0, 0));
                tempConfig_.appearance.theme = (themeSel == 0) ? "dark" : "light";
            }
            HWND hLangCombo = GetDlgItem(hwnd, IDC_COMBO_LANGUAGE);
            int sel = static_cast<int>(SendMessage(hLangCombo, CB_GETCURSEL, 0, 0));
            ReadMetricSelectionsFromList(hwnd);
            tempConfig_.general.language = (sel == 1) ? "en" : "zh";
            ApplyLanguage(hwnd);
            SetupReadingsList(hwnd);
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
    SetDlgItemText(hwnd, IDC_GROUP_METRICS, Text(L"\u68C0\u6D4B\u5230\u7684\u4F20\u611F\u5668", L"Detected Sensors").c_str());
    SetDlgItemText(hwnd, IDC_GROUP_APPEAR, Text(L"\u754C\u9762", L"Appearance").c_str());
    SetDlgItemText(hwnd, IDC_LABEL_THEME, Text(L"\u4E3B\u9898\uFF1A", L"Theme:").c_str());
    SetDlgItemText(hwnd, IDC_LABEL_LANGUAGE, Text(L"\u8BED\u8A00\uFF1A", L"Language:").c_str());
    SetDlgItemText(hwnd, IDC_LABEL_INTERVAL, Text(L"\u66F4\u65B0\u9891\u7387\uFF1A", L"Update:").c_str());
    SetDlgItemText(hwnd, IDC_GROUP_DATA, Text(L"\u6570\u636E\u6765\u6E90", L"Data Source").c_str());
    SetDlgItemText(hwnd, IDC_BTN_APPLY, Text(L"\u5E94\u7528", L"Apply").c_str());
    SetDlgItemText(hwnd, IDC_BTN_CANCEL, Text(L"\u53D6\u6D88", L"Cancel").c_str());
    SetDlgItemText(hwnd, IDC_BTN_CONNECT_VR, Text(L"\u8FDE\u63A5 SteamVR", L"Connect SteamVR").c_str());

    HWND hThemeCombo = GetDlgItem(hwnd, IDC_COMBO_THEME);
    if (hThemeCombo) {
        const auto theme = tempConfig_.appearance.theme;
        SendMessage(hThemeCombo, CB_RESETCONTENT, 0, 0);
        SendMessage(hThemeCombo, CB_ADDSTRING, 0,
                    reinterpret_cast<LPARAM>(Text(L"\u6DF1\u8272", L"Dark").c_str()));
        SendMessage(hThemeCombo, CB_ADDSTRING, 0,
                    reinterpret_cast<LPARAM>(Text(L"\u6D45\u8272", L"Light").c_str()));
        tempConfig_.appearance.theme = theme;
        SendMessage(hThemeCombo, CB_SETCURSEL, theme == "dark" ? 0 : 1, 0);
    }

    HWND hLangCombo = GetDlgItem(hwnd, IDC_COMBO_LANGUAGE);
    if (hLangCombo) {
        SendMessage(hLangCombo, CB_RESETCONTENT, 0, 0);
        SendMessage(hLangCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"\u4E2D\u6587"));
        SendMessage(hLangCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"English"));
        SendMessage(hLangCombo, CB_SETCURSEL, tempConfig_.general.language == "en" ? 1 : 0, 0);
    }
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

void SettingsWindow::SetupReadingsList(HWND hwnd)
{
    HWND hList = GetDlgItem(hwnd, IDC_LIST_METRICS);
    if (!hList) {
        return;
    }

    ListView_DeleteAllItems(hList);
    while (ListView_DeleteColumn(hList, 0)) {
    }

    ListView_SetExtendedListViewStyle(
        hList,
        LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    struct Column {
        const wchar_t* zh;
        const wchar_t* en;
        int width;
    };

    const Column columns[] = {
        {L"\u7C7B\u578B", L"Metric", 90},
        {L"GPU / \u8BBE\u5907", L"GPU / Device", 175},
        {L"\u6570\u503C", L"Value", 75},
        {L"\u5355\u4F4D", L"Unit", 50},
        {L"\u6765\u6E90", L"Source", 105},
        {L"\u539F\u59CB\u6807\u7B7E", L"Raw Label", 150},
    };

    for (int i = 0; i < static_cast<int>(std::size(columns)); ++i) {
        LVCOLUMNW column = {};
        column.mask = LVCF_TEXT | LVCF_WIDTH;
        auto title = Text(columns[i].zh, columns[i].en);
        column.pszText = title.data();
        column.cx = columns[i].width;
        ListView_InsertColumn(hList, i, &column);
    }
}

void SettingsWindow::UpdateInfo(HWND hwnd)
{
    ReadMetricSelectionsFromList(hwnd);

    const auto newReadings = readingsProvider_
        ? readingsProvider_(tempConfig_.data.hardwareSource)
        : std::vector<SensorReading>{};

    HWND hList = GetDlgItem(hwnd, IDC_LIST_METRICS);
    if (!hList) {
        latestReadings_ = newReadings;
        return;
    }

    const int topIndex = ListView_GetTopIndex(hList);
    SendMessage(hList, WM_SETREDRAW, FALSE, 0);

    if (!HasSameReadingRows(newReadings)) {
        ListView_DeleteAllItems(hList);
        latestReadings_ = newReadings;

        for (int i = 0; i < static_cast<int>(latestReadings_.size()); ++i) {
            InsertReadingRow(hList, i, latestReadings_[i]);
        }

        if (topIndex > 0 && topIndex < ListView_GetItemCount(hList)) {
            ListView_EnsureVisible(hList, topIndex, FALSE);
        }
    } else {
        latestReadings_ = newReadings;
        for (int i = 0; i < static_cast<int>(latestReadings_.size()); ++i) {
            UpdateReadingRow(hList, i, latestReadings_[i]);
        }
    }

    SendMessage(hList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hList, nullptr, FALSE);
}

bool SettingsWindow::HasSameReadingRows(
    const std::vector<SensorReading>& readings) const
{
    if (readings.size() != latestReadings_.size()) {
        return false;
    }

    for (size_t i = 0; i < readings.size(); ++i) {
        if (SensorReadingKey(readings[i]) != SensorReadingKey(latestReadings_[i])) {
            return false;
        }
    }

    return true;
}

void SettingsWindow::InsertReadingRow(HWND hList,
                                      int row,
                                      const SensorReading& reading)
{
    auto category = CategoryName(reading.category);
    LVITEMW item = {};
    item.mask = LVIF_TEXT | LVIF_PARAM;
    item.iItem = row;
    item.pszText = category.data();
    item.lParam = static_cast<LPARAM>(row);
    const int inserted = ListView_InsertItem(hList, &item);
    if (inserted >= 0) {
        UpdateReadingRow(hList, inserted, reading);
    }
}

void SettingsWindow::UpdateReadingRow(HWND hList,
                                      int row,
                                      const SensorReading& reading)
{
    auto category = CategoryName(reading.category);
    auto device = ToWide(reading.device);
    auto value = FormatReading(reading);
    auto unit = ToWide(reading.unit);
    auto source = ToWide(reading.source);
    auto label = ToWide(reading.label);

    LVITEMW item = {};
    item.mask = LVIF_TEXT | LVIF_PARAM;
    item.iItem = row;
    item.iSubItem = 0;
    item.pszText = category.data();
    item.lParam = static_cast<LPARAM>(row);
    ListView_SetItem(hList, &item);

    ListView_SetItemText(hList, row, 1, const_cast<wchar_t*>(device.c_str()));
    ListView_SetItemText(hList, row, 2, const_cast<wchar_t*>(value.c_str()));
    ListView_SetItemText(hList, row, 3, const_cast<wchar_t*>(unit.c_str()));
    ListView_SetItemText(hList, row, 4, const_cast<wchar_t*>(source.c_str()));
    ListView_SetItemText(hList, row, 5, const_cast<wchar_t*>(label.c_str()));
    ListView_SetCheckState(hList, row, IsReadingEnabled(reading));
}

void SettingsWindow::ReadMetricSelectionsFromList(HWND hwnd)
{
    HWND hList = GetDlgItem(hwnd, IDC_LIST_METRICS);
    if (!hList) {
        return;
    }

    const int itemCount = ListView_GetItemCount(hList);
    if (itemCount <= 0 || latestReadings_.empty()) {
        return;
    }

    std::vector<MetricConfig> selected;
    for (int i = 0; i < itemCount; ++i) {
        if (!ListView_GetCheckState(hList, i)) {
            continue;
        }

        LVITEMW item = {};
        item.mask = LVIF_PARAM;
        item.iItem = i;
        if (!ListView_GetItem(hList, &item)) {
            continue;
        }

        const auto index = static_cast<size_t>(item.lParam);
        if (index >= latestReadings_.size()) {
            continue;
        }

        selected.push_back(MetricFromReading(latestReadings_[index]));
    }

    tempConfig_.metrics = std::move(selected);
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
        if (reading.unit == "%") {
            ss << std::fixed << std::setprecision(0) << reading.value << L"%";
        } else if (reading.unit.empty() || reading.unit == "MB") {
            if (reading.value > 1024.0) {
                ss << std::fixed << std::setprecision(1) << (reading.value / 1024.0) << L" GB";
            } else {
                ss << std::fixed << std::setprecision(0) << reading.value << L" MB";
            }
        } else {
            ss << std::fixed << std::setprecision(1) << reading.value
               << L" " << ToWide(reading.unit);
        }
        break;
    case SensorCategory::GpuFan:
    case SensorCategory::Fan:
        if (reading.unit == "%") {
            ss << std::fixed << std::setprecision(0) << reading.value << L"%";
        } else {
            ss << std::fixed << std::setprecision(0) << reading.value << L" RPM";
        }
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
            ss << L" " << ToWide(reading.unit);
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
    default: return Text(L"\u5176\u4ED6", L"Other");
    }
}

std::wstring SettingsWindow::ToWide(const std::string& text) const
{
    if (text.empty()) {
        return {};
    }

    int length = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.c_str(),
        static_cast<int>(text.size()),
        nullptr,
        0);
    UINT codePage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    if (length <= 0) {
        codePage = CP_ACP;
        flags = 0;
        length = MultiByteToWideChar(
            codePage,
            flags,
            text.c_str(),
            static_cast<int>(text.size()),
            nullptr,
            0);
    }

    if (length <= 0) {
        return std::wstring(text.begin(), text.end());
    }

    std::wstring wide(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(
        codePage,
        flags,
        text.c_str(),
        static_cast<int>(text.size()),
        wide.data(),
        length);
    return wide;
}

std::string SettingsWindow::ToSafeUtf8(const std::string& text) const
{
    const auto wide = ToWide(text);
    if (wide.empty()) {
        return {};
    }

    int length = WideCharToMultiByte(
        CP_UTF8,
        0,
        wide.c_str(),
        static_cast<int>(wide.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (length <= 0) {
        return {};
    }

    std::string utf8(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        wide.c_str(),
        static_cast<int>(wide.size()),
        utf8.data(),
        length,
        nullptr,
        nullptr);
    return utf8;
}

bool SettingsWindow::IsReadingEnabled(const SensorReading& reading) const
{
    const bool hasExactSelection = std::any_of(
        tempConfig_.metrics.begin(),
        tempConfig_.metrics.end(),
        [](const MetricConfig& metric) {
            return metric.enabled && !metric.sensorKey.empty();
        });

    for (const auto& metric : tempConfig_.metrics) {
        if (!metric.enabled) {
            continue;
        }
        if (!metric.sensorKey.empty()) {
            if (metric.sensorKey == SensorReadingKey(reading) ||
                metric.sensorKey == LegacySensorReadingKey(reading)) {
                return true;
            }
            continue;
        }
        if (!hasExactSelection &&
            metric.category == SensorCategoryKey(reading.category)) {
            return true;
        }
    }

    return false;
}

MetricConfig SettingsWindow::MetricFromReading(const SensorReading& reading) const
{
    MetricConfig metric;
    metric.category = SensorCategoryKey(reading.category);
    metric.enabled = true;
    metric.source = ToSafeUtf8(reading.source);
    metric.sensorId = reading.sensorId;
    metric.readingId = reading.readingId;
    metric.sensorKey = SensorReadingKey(reading);

    metric.label = ToSafeUtf8(reading.label);
    if (!reading.device.empty()) {
        const auto separator = reading.device.find(':');
        const auto gpuLabel = separator == std::string::npos
            ? reading.device
            : reading.device.substr(0, separator);
        metric.label = ToSafeUtf8(gpuLabel) + " " + ToSafeUtf8(reading.label);
    }

    return metric;
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
