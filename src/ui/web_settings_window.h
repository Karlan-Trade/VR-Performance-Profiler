#pragma once

#include "core/config.h"
#include "data/sensor_data.h"

#include <Windows.h>
#include <functional>
#include <string>
#include <vector>

namespace vrperf {

class WebSettingsWindow {
public:
    using ReadingsProvider = std::function<std::vector<SensorReading>(const std::string&)>;
    using ApplyCallback = std::function<void()>;
    using ConnectCallback = std::function<bool()>;

    WebSettingsWindow();
    ~WebSettingsWindow();

    bool Show(HWND ownerHwnd,
              Config& config,
              ReadingsProvider readingsProvider,
              ApplyCallback applyCallback,
              ConnectCallback connectCallback);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool CreateHostWindow(HWND ownerHwnd);
    bool InitializeWebView();
    void ResizeWebView();
    void ApplyWindowFrameTheme();
    void ShutdownWebView();
    void OnWebMessage(const std::wstring& messageJson);
    void SendState();
    void SendStatus(const std::string& message, bool ok);
    void ApplyFromJson(const std::wstring& messageJson);
    void RefreshReadings();

    std::wstring BuildHtml() const;
    std::wstring BuildStateJson() const;
    std::wstring BuildReadingsJson() const;
    std::wstring BuildConfigJson() const;
    std::wstring GetUserDataFolder() const;

    MetricConfig MetricFromReading(const SensorReading& reading) const;
    bool IsReadingEnabled(const SensorReading& reading) const;

    HWND hwnd_ = nullptr;
    HWND ownerHwnd_ = nullptr;
    Config* config_ = nullptr;
    Config tempConfig_;
    ReadingsProvider readingsProvider_;
    ApplyCallback applyCallback_;
    ConnectCallback connectCallback_;
    std::vector<SensorReading> latestReadings_;
    bool closed_ = false;
    bool initFailed_ = false;

    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace vrperf
