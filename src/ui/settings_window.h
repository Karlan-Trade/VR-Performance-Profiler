#pragma once

#include "core/config.h"
#include "hwinfo/sensor_data.h"

#include <Windows.h>
#include <functional>
#include <string>
#include <vector>

namespace vrperf {

class SettingsWindow {
public:
    SettingsWindow();
    ~SettingsWindow();

    using ReadingsProvider = std::function<std::vector<SensorReading>()>;
    using ApplyCallback = std::function<void()>;
    using ConnectCallback = std::function<bool()>;

    // Show the settings dialog (modal)
    bool Show(HWND parentHwnd,
              Config& config,
              ReadingsProvider readingsProvider = {},
              ApplyCallback applyCallback = {},
              ConnectCallback connectCallback = {});

private:
    static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    INT_PTR HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void OnInitDialog(HWND hwnd);
    void OnApply(HWND hwnd);
    void OnCancel(HWND hwnd);
    void OnConnectSteamVr(HWND hwnd);
    void OnTimer(HWND hwnd, WPARAM timerId);
    void OnCommand(HWND hwnd, WPARAM wParam);
    void ApplyLanguage(HWND hwnd);
    void PopulateCombos(HWND hwnd);
    void UpdateInfo(HWND hwnd);
    void SetupReadingsList(HWND hwnd);
    void ReadMetricSelectionsFromList(HWND hwnd);
    bool HasSameReadingRows(const std::vector<SensorReading>& readings) const;
    void InsertReadingRow(HWND hList, int row, const SensorReading& reading);
    void UpdateReadingRow(HWND hList, int row, const SensorReading& reading);

    std::wstring Text(const wchar_t* zh, const wchar_t* en) const;
    std::wstring FormatReading(const SensorReading& reading) const;
    std::wstring CategoryName(SensorCategory category) const;
    std::wstring ToWide(const std::string& text) const;
    std::string ToSafeUtf8(const std::string& text) const;
    bool IsReadingEnabled(const SensorReading& reading) const;
    MetricConfig MetricFromReading(const SensorReading& reading) const;
    int SelectedUpdateIntervalMs(HWND hwnd) const;
    void SelectUpdateInterval(HWND hwnd, int intervalMs);

    Config* config_ = nullptr;
    Config tempConfig_;  // Working copy for editing
    ReadingsProvider readingsProvider_;
    ApplyCallback applyCallback_;
    ConnectCallback connectCallback_;
    std::vector<SensorReading> latestReadings_;
};

} // namespace vrperf
