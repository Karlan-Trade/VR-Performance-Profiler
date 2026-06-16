#include "ui/web_settings_window.h"

#ifdef VRPERF_WITH_WEBVIEW2

#include "core/log.h"

#include <WebView2.h>
#include <dwmapi.h>
#include <nlohmann/json.hpp>
#include <wrl.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace vrperf {

namespace {

constexpr UINT_PTR kRefreshTimerId = 41;
constexpr UINT kRefreshIntervalMs = 1000;
constexpr DWORD kDwmwaUseImmersiveDarkModeBefore20H1 = 19;

std::wstring ToWide(const std::string& text)
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

std::string ToUtf8(const std::wstring& text)
{
    if (text.empty()) {
        return {};
    }

    int length = WideCharToMultiByte(
        CP_UTF8,
        0,
        text.c_str(),
        static_cast<int>(text.size()),
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
        text.c_str(),
        static_cast<int>(text.size()),
        utf8.data(),
        length,
        nullptr,
        nullptr);
    return utf8;
}

std::string ToSafeUtf8(const std::string& text)
{
    return ToUtf8(ToWide(text));
}

std::wstring JsonToWide(const nlohmann::json& value)
{
    return ToWide(value.dump(
        -1,
        ' ',
        false,
        nlohmann::json::error_handler_t::replace));
}

bool IsDarkTheme(const Config& config)
{
    return config.appearance.theme != "light";
}

std::wstring FormatReadingValue(const SensorReading& reading)
{
    if (!std::isfinite(reading.value)) {
        return L"--";
    }

    std::wstringstream ss;
    switch (reading.category) {
    case SensorCategory::CpuTemp:
    case SensorCategory::GpuTemp:
        ss << std::fixed << std::setprecision(0) << reading.value << L" C";
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
            ss << std::fixed << std::setprecision(1) << reading.value << L" " << ToWide(reading.unit);
        }
        break;
    case SensorCategory::GpuFan:
    case SensorCategory::Fan:
        ss << std::fixed << std::setprecision(0) << reading.value
           << (reading.unit == "%" ? L"%" : L" RPM");
        break;
    case SensorCategory::Power:
        ss << std::fixed << std::setprecision(1) << reading.value << L" W";
        break;
    case SensorCategory::Voltage:
        ss << std::fixed << std::setprecision(2) << reading.value << L" V";
        break;
    case SensorCategory::VrFps:
        ss << std::fixed << std::setprecision(1) << reading.value << L" FPS";
        break;
    case SensorCategory::VrFrameTime:
    case SensorCategory::VrGpuFrameTime:
        ss << std::fixed << std::setprecision(1) << reading.value << L" ms";
        break;
    case SensorCategory::VrRefreshRate:
        ss << std::fixed << std::setprecision(0) << reading.value << L" Hz";
        break;
    case SensorCategory::VrDroppedFrames:
        ss << std::fixed << std::setprecision(0) << reading.value;
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

std::string CategoryKey(SensorCategory category)
{
    return SensorCategoryKey(category);
}

std::string CategoryName(SensorCategory category)
{
    switch (category) {
    case SensorCategory::CpuLoad: return "CPU Load";
    case SensorCategory::CpuTemp: return "CPU Temp";
    case SensorCategory::CpuClock: return "CPU Clock";
    case SensorCategory::GpuLoad: return "GPU Load";
    case SensorCategory::GpuTemp: return "GPU Temp";
    case SensorCategory::GpuClock: return "GPU Clock";
    case SensorCategory::GpuMemory: return "VRAM";
    case SensorCategory::GpuFan: return "GPU Fan";
    case SensorCategory::RamUsage: return "RAM";
    case SensorCategory::Fan: return "Fan";
    case SensorCategory::Voltage: return "Voltage";
    case SensorCategory::Power: return "Power";
    case SensorCategory::VrFps: return "VR FPS";
    case SensorCategory::VrFrameTime: return "VR Frame";
    case SensorCategory::VrGpuFrameTime: return "VR GPU";
    case SensorCategory::VrRefreshRate: return "VR Refresh";
    case SensorCategory::VrDroppedFrames: return "VR Drops";
    default: return "Other";
    }
}

} // namespace

struct WebSettingsWindow::Impl {
    ComPtr<ICoreWebView2Environment> environment;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
    EventRegistrationToken messageToken = {};
};

WebSettingsWindow::WebSettingsWindow()
    : impl_(new Impl())
{
}

WebSettingsWindow::~WebSettingsWindow()
{
    ShutdownWebView();
    delete impl_;
    impl_ = nullptr;
}

bool WebSettingsWindow::Show(HWND ownerHwnd,
                             Config& config,
                             ReadingsProvider readingsProvider,
                             ApplyCallback applyCallback,
                             ConnectCallback connectCallback)
{
    ownerHwnd_ = ownerHwnd;
    config_ = &config;
    tempConfig_ = config;
    readingsProvider_ = std::move(readingsProvider);
    applyCallback_ = std::move(applyCallback);
    connectCallback_ = std::move(connectCallback);
    latestReadings_.clear();
    closed_ = false;
    initFailed_ = false;

    const HRESULT coHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninitialize = SUCCEEDED(coHr);

    if (!CreateHostWindow(ownerHwnd) || !InitializeWebView()) {
        if (hwnd_) {
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
        }
        if (shouldUninitialize) {
            CoUninitialize();
        }
        return false;
    }

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    MSG msg = {};
    bool sawQuitMessage = false;
    while (!closed_) {
        const BOOL result = GetMessage(&msg, nullptr, 0, 0);
        if (result == 0) {
            sawQuitMessage = true;
            closed_ = true;
            break;
        }
        if (result < 0) {
            initFailed_ = true;
            closed_ = true;
            break;
        }
        if (!IsDialogMessage(hwnd_, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    ShutdownWebView();
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (shouldUninitialize) {
        CoUninitialize();
    }
    if (sawQuitMessage) {
        PostQuitMessage(static_cast<int>(msg.wParam));
    }
    return !initFailed_;
}

bool WebSettingsWindow::CreateHostWindow(HWND ownerHwnd)
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WebSettingsWindow::WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(wc.hInstance, MAKEINTRESOURCE(101));
    wc.hIconSm = LoadIcon(wc.hInstance, MAKEINTRESOURCE(101));
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"VRPerfProfilerWebSettings";
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"VR Performance Profiler",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1120,
        740,
        nullptr,
        nullptr,
        wc.hInstance,
        this);
    ApplyWindowFrameTheme();
    return hwnd_ != nullptr;
}

bool WebSettingsWindow::InitializeWebView()
{
    const auto userDataFolder = GetUserDataFolder();
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(userDataFolder), ec);

    auto handler = Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
        [this](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
            if (FAILED(result) || !environment) {
                initFailed_ = true;
                LogInfo("WebView2 environment creation failed");
                DestroyWindow(hwnd_);
                return S_OK;
            }

            impl_->environment = environment;
            auto controllerHandler = Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                [this](HRESULT controllerResult, ICoreWebView2Controller* controller) -> HRESULT {
                    if (FAILED(controllerResult) || !controller) {
                        initFailed_ = true;
                        LogInfo("WebView2 controller creation failed");
                        DestroyWindow(hwnd_);
                        return S_OK;
                    }

                    impl_->controller = controller;
                    impl_->controller->get_CoreWebView2(&impl_->webview);
                    ResizeWebView();

                    impl_->webview->add_WebMessageReceived(
                        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                            [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                LPWSTR rawJson = nullptr;
                                if (SUCCEEDED(args->get_WebMessageAsJson(&rawJson)) && rawJson) {
                                    OnWebMessage(rawJson);
                                    CoTaskMemFree(rawJson);
                                }
                                return S_OK;
                            }).Get(),
                        &impl_->messageToken);

                    RefreshReadings();
                    impl_->webview->NavigateToString(BuildHtml().c_str());
                    SendState();
                    SetTimer(hwnd_, kRefreshTimerId, kRefreshIntervalMs, nullptr);
                    return S_OK;
                });
            environment->CreateCoreWebView2Controller(hwnd_, controllerHandler.Get());
            return S_OK;
        });

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        userDataFolder.c_str(),
        nullptr,
        handler.Get());
    if (FAILED(hr)) {
        LogInfo("CreateCoreWebView2EnvironmentWithOptions failed");
        return false;
    }
    return true;
}

void WebSettingsWindow::ShutdownWebView()
{
    if (hwnd_) {
        KillTimer(hwnd_, kRefreshTimerId);
    }
    if (impl_ && impl_->webview) {
        impl_->webview->remove_WebMessageReceived(impl_->messageToken);
    }
    if (impl_ && impl_->controller) {
        impl_->controller->Close();
    }
    if (impl_) {
        impl_->webview.Reset();
        impl_->controller.Reset();
        impl_->environment.Reset();
    }
}

void WebSettingsWindow::ResizeWebView()
{
    if (!impl_ || !impl_->controller || !hwnd_) {
        return;
    }
    RECT bounds = {};
    GetClientRect(hwnd_, &bounds);
    impl_->controller->put_Bounds(bounds);
}

void WebSettingsWindow::ApplyWindowFrameTheme()
{
    if (!hwnd_) {
        return;
    }

    const BOOL useDarkFrame = IsDarkTheme(tempConfig_) ? TRUE : FALSE;
    HRESULT hr = DwmSetWindowAttribute(
        hwnd_,
        DWMWA_USE_IMMERSIVE_DARK_MODE,
        &useDarkFrame,
        sizeof(useDarkFrame));
    if (FAILED(hr)) {
        DwmSetWindowAttribute(
            hwnd_,
            kDwmwaUseImmersiveDarkModeBefore20H1,
            &useDarkFrame,
            sizeof(useDarkFrame));
    }

    RedrawWindow(
        hwnd_,
        nullptr,
        nullptr,
        RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW);
}

LRESULT CALLBACK WebSettingsWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WebSettingsWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<WebSettingsWindow*>(create->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<WebSettingsWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->HandleMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT WebSettingsWindow::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_SIZE:
        ResizeWebView();
        return 0;
    case WM_TIMER:
        if (wParam == kRefreshTimerId) {
            RefreshReadings();
            SendState();
            return 0;
        }
        break;
    case WM_CLOSE:
        closed_ = true;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        closed_ = true;
        hwnd_ = nullptr;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void WebSettingsWindow::OnWebMessage(const std::wstring& messageJson)
{
    nlohmann::json message;
    try {
        message = nlohmann::json::parse(ToUtf8(messageJson));
    } catch (...) {
        return;
    }

    const auto type = message.value("type", "");
    if (type == "ready" || type == "refresh") {
        if (type == "refresh") {
            const auto hardwareSource = message.value("hardwareSource", tempConfig_.data.hardwareSource);
            tempConfig_.data.hardwareSource = hardwareSource == "hwinfo" ? "hwinfo" : "afterburner";
        }
        RefreshReadings();
        SendState();
    } else if (type == "previewTheme") {
        tempConfig_.appearance.theme = message.value("theme", tempConfig_.appearance.theme);
        ApplyWindowFrameTheme();
    } else if (type == "apply") {
        ApplyFromJson(messageJson);
        SendStatus("settingsApplied", true);
        SendState();
    } else if (type == "connect") {
        ApplyFromJson(messageJson);
        const bool ok = connectCallback_ && connectCallback_();
        SendStatus(ok ? "steamVrConnected" : "steamVrConnectFailed", ok);
        SendState();
    } else if (type == "close") {
        closed_ = true;
        DestroyWindow(hwnd_);
    }
}

void WebSettingsWindow::ApplyFromJson(const std::wstring& messageJson)
{
    nlohmann::json message;
    try {
        message = nlohmann::json::parse(ToUtf8(messageJson));
    } catch (...) {
        return;
    }

    tempConfig_.overlay.mode = message.value("mode", tempConfig_.overlay.mode);
    tempConfig_.overlay.widthMeters = (std::clamp)(
        message.value("overlayWidthMeters", tempConfig_.overlay.widthMeters),
        0.5f,
        2.5f);
    const auto wristHand = message.value("wristHand", tempConfig_.wrist.hand);
    tempConfig_.wrist.hand = wristHand == "right" ? "right" : "left";
    tempConfig_.appearance.theme = message.value("theme", tempConfig_.appearance.theme);
    ApplyWindowFrameTheme();
    tempConfig_.general.language = message.value("language", tempConfig_.general.language);
    const auto hardwareSource = message.value("hardwareSource", tempConfig_.data.hardwareSource);
    tempConfig_.data.hardwareSource = hardwareSource == "hwinfo" ? "hwinfo" : "afterburner";
    tempConfig_.overlay.updateIntervalMs =
        message.value("updateIntervalMs", tempConfig_.overlay.updateIntervalMs);

    std::vector<MetricConfig> selected;
    const auto keys = message.value("selectedKeys", std::vector<std::string>{});
    for (const auto& key : keys) {
        auto it = std::find_if(
            latestReadings_.begin(),
            latestReadings_.end(),
            [&](const SensorReading& reading) {
                return SensorReadingKey(reading) == key;
            });
        if (it != latestReadings_.end()) {
            selected.push_back(MetricFromReading(*it));
        }
    }
    tempConfig_.metrics = std::move(selected);

    *config_ = tempConfig_;
    config_->Save();
    if (applyCallback_) {
        applyCallback_();
    }
}

void WebSettingsWindow::RefreshReadings()
{
    latestReadings_ = readingsProvider_
        ? readingsProvider_(tempConfig_.data.hardwareSource)
        : std::vector<SensorReading>{};
}

void WebSettingsWindow::SendState()
{
    if (impl_ && impl_->webview) {
        impl_->webview->PostWebMessageAsJson(BuildStateJson().c_str());
    }
}

void WebSettingsWindow::SendStatus(const std::string& message, bool ok)
{
    if (!impl_ || !impl_->webview) {
        return;
    }
    nlohmann::json payload = {
        {"type", "status"},
        {"ok", ok},
        {"message", ToSafeUtf8(message)},
    };
    impl_->webview->PostWebMessageAsJson(JsonToWide(payload).c_str());
}

std::wstring WebSettingsWindow::BuildStateJson() const
{
    nlohmann::json payload = {
        {"type", "state"},
        {"config", nlohmann::json::parse(ToUtf8(BuildConfigJson()))},
        {"readings", nlohmann::json::parse(ToUtf8(BuildReadingsJson()))},
    };
    return JsonToWide(payload);
}

std::wstring WebSettingsWindow::BuildConfigJson() const
{
    std::vector<std::string> selectedKeys;
    for (const auto& metric : tempConfig_.metrics) {
        if (metric.enabled && !metric.sensorKey.empty()) {
            selectedKeys.push_back(metric.sensorKey);
        }
    }
    if (selectedKeys.empty()) {
        for (const auto& reading : latestReadings_) {
            if (IsReadingEnabled(reading)) {
                selectedKeys.push_back(SensorReadingKey(reading));
            }
        }
    }

    nlohmann::json config = {
        {"mode", tempConfig_.overlay.mode},
        {"overlayWidthMeters", tempConfig_.overlay.widthMeters},
        {"wristHand", tempConfig_.wrist.hand},
        {"theme", tempConfig_.appearance.theme},
        {"language", tempConfig_.general.language},
        {"hardwareSource", tempConfig_.data.hardwareSource},
        {"updateIntervalMs", tempConfig_.overlay.updateIntervalMs},
        {"selectedKeys", selectedKeys},
    };
    return JsonToWide(config);
}

std::wstring WebSettingsWindow::BuildReadingsJson() const
{
    nlohmann::json rows = nlohmann::json::array();
    for (const auto& reading : latestReadings_) {
        rows.push_back({
            {"key", SensorReadingKey(reading)},
            {"category", CategoryKey(reading.category)},
            {"categoryName", CategoryName(reading.category)},
            {"device", ToSafeUtf8(reading.device)},
            {"label", ToSafeUtf8(reading.label)},
            {"value", ToUtf8(FormatReadingValue(reading))},
            {"unit", ToSafeUtf8(reading.unit)},
            {"source", ToSafeUtf8(reading.source)},
            {"selected", IsReadingEnabled(reading)},
        });
    }
    return JsonToWide(rows);
}

MetricConfig WebSettingsWindow::MetricFromReading(const SensorReading& reading) const
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
        const auto deviceName = separator == std::string::npos
            ? reading.device
            : reading.device.substr(0, separator);
        metric.label = ToSafeUtf8(deviceName) + " " + ToSafeUtf8(reading.label);
    }
    return metric;
}

bool WebSettingsWindow::IsReadingEnabled(const SensorReading& reading) const
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

std::wstring WebSettingsWindow::GetUserDataFolder() const
{
    wchar_t localAppData[MAX_PATH] = {};
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH) == 0) {
        return L".webview2";
    }
    return (std::filesystem::path(localAppData) / L"VRPerfProfiler" / L"WebView2").wstring();
}

std::wstring WebSettingsWindow::BuildHtml() const
{
    return LR"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body[data-theme="dark"] { color-scheme: dark; --bg:#101418; --sidebar:#121820; --panel:#171d23; --panelHead:#131a21; --control:#10161d; --control2:#1b232c; --seg:#0d1116; --segActive:#12324a; --line:#2b333d; --text:#edf2f7; --muted:#9aa7b5; --accent:#37a2ff; --primaryText:#071018; --ok:#39c980; --okBg:#153823; --okLine:#276b42; --okText:#c7f8dc; --warn:#f2b84b; --hover:#1b232b; }
    body[data-theme="light"] { color-scheme: light; --bg:#f4f6f8; --sidebar:#ffffff; --panel:#ffffff; --panelHead:#eef2f6; --control:#ffffff; --control2:#f8fafc; --seg:#eef2f6; --segActive:#d9ecff; --line:#cfd8e3; --text:#17202a; --muted:#667789; --accent:#126db3; --primaryText:#ffffff; --ok:#16784a; --okBg:#e7f6ee; --okLine:#9bd4b7; --okText:#115c38; --warn:#9a6700; --hover:#eef5fb; }
    * { box-sizing: border-box; }
    body { margin:0; font:14px/1.4 "Segoe UI", system-ui, sans-serif; background:var(--bg); color:var(--text); }
    .app { display:grid; grid-template-columns:260px 1fr; height:100vh; }
    aside { border-right:1px solid var(--line); padding:20px; background:var(--sidebar); }
    main { padding:20px 24px; overflow:auto; }
    h1 { font-size:20px; font-weight:600; margin:0 0 18px; }
    h2 { font-size:13px; text-transform:uppercase; letter-spacing:0; color:var(--muted); margin:22px 0 10px; }
    .seg { display:grid; grid-template-columns:1fr 1fr; gap:6px; padding:4px; background:var(--seg); border:1px solid var(--line); border-radius:8px; }
    .seg button, .button { border:1px solid var(--line); background:var(--control2); color:var(--text); border-radius:6px; height:34px; padding:0 12px; cursor:pointer; }
    .seg button.active { border-color:var(--accent); background:var(--segActive); }
    label { display:block; color:var(--muted); margin:12px 0 6px; }
    select { width:100%; height:34px; border-radius:6px; border:1px solid var(--line); background:var(--control); color:var(--text); padding:0 10px; }
    input[type=range] { width:100%; accent-color:var(--accent); }
    .rangeRow { display:grid; grid-template-columns:1fr auto; gap:10px; align-items:center; }
    .valueBadge { min-width:54px; text-align:right; color:var(--text); font-variant-numeric:tabular-nums; }
    .actions { display:flex; gap:8px; margin-top:18px; }
    .button.primary { background:var(--accent); border-color:var(--accent); color:var(--primaryText); font-weight:600; }
    .button.ok { background:var(--okBg); border-color:var(--okLine); color:var(--okText); }
    .status { min-height:20px; color:var(--muted); margin-top:12px; }
    .status.good { color:var(--ok); } .status.bad { color:var(--warn); }
    .toolbar { display:flex; justify-content:space-between; align-items:center; gap:12px; margin-bottom:12px; }
    .search { width:300px; max-width:40vw; height:34px; border:1px solid var(--line); border-radius:6px; background:var(--control); color:var(--text); padding:0 10px; }
    table { width:100%; border-collapse:collapse; background:var(--panel); border:1px solid var(--line); border-radius:8px; overflow:hidden; }
    th, td { padding:9px 10px; border-bottom:1px solid var(--line); text-align:left; white-space:nowrap; }
    th { color:var(--muted); font-weight:600; background:var(--panelHead); position:sticky; top:0; }
    tr:hover td { background:var(--hover); }
    td:nth-child(4), td:nth-child(6) { max-width:260px; overflow:hidden; text-overflow:ellipsis; }
    input[type=checkbox] { width:16px; height:16px; }
    .empty { color:var(--muted); padding:28px; text-align:center; border:1px dashed var(--line); border-radius:8px; }
  </style>
</head>
<body data-theme="dark">
  <div class="app">
    <aside>
      <h1 id="title">VR Performance Profiler</h1>
      <h2 id="overlayHeading">Overlay</h2>
      <div class="seg">
        <button id="modeHud">HUD</button>
        <button id="modeWrist">Wrist</button>
      </div>
      <label id="wristHandLabel" for="handLeft">Wrist hand</label>
      <div class="seg">
        <button id="handLeft">Left</button>
        <button id="handRight">Right</button>
      </div>
      <label id="panelSizeLabel" for="panelSize">HUD panel size</label>
      <div class="rangeRow">
        <input id="panelSize" type="range" min="0.5" max="2.5" step="0.05">
        <span id="panelSizeValue" class="valueBadge">1.50 m</span>
      </div>
      <h2 id="appearanceHeading">Appearance</h2>
      <label id="themeLabel" for="theme">Theme</label>
      <select id="theme"><option value="dark">Dark</option><option value="light">Light</option></select>
      <label id="languageLabel" for="language">Language</label>
      <select id="language"><option value="zh">Chinese</option><option value="en">English</option></select>
      <h2 id="dataHeading">Data</h2>
      <label id="hardwareSourceLabel" for="hardwareSource">Primary data source</label>
      <select id="hardwareSource"><option value="afterburner">MSI Afterburner</option><option value="hwinfo">HWiNFO</option></select>
      <label id="intervalLabel" for="interval">Update interval</label>
      <select id="interval"><option value="66">66 ms</option><option value="250">250 ms</option><option value="500">500 ms</option><option value="1000">1000 ms</option><option value="2000">2000 ms</option></select>
      <div class="actions"><button class="button primary" id="apply">Apply</button><button class="button ok" id="connect">Connect SteamVR</button></div>
      <div id="status" class="status"></div>
    </aside>
    <main>
      <div class="toolbar">
        <div><h1 id="sensorsHeading" style="margin:0">Detected Sensors</h1><div id="sensorHint" class="status">Select exact readings to show in the VR overlay.</div></div>
        <input class="search" id="filter" placeholder="Filter sensors">
      </div>
      <div id="table"></div>
    </main>
  </div>
  <script>
    const state = { config:{mode:'hud', overlayWidthMeters:1.5, wristHand:'left', theme:'dark', language:'zh', hardwareSource:'afterburner', selectedKeys:[]}, readings:[], filter:'', initialized:false, statusMessage:'', statusOk:null };
    const $ = id => document.getElementById(id);
    const strings = {
      en: {
        title:'VR Performance Profiler', overlayHeading:'Overlay', appearanceHeading:'Appearance', dataHeading:'Data',
        hud:'HUD', wrist:'Wrist', wristHand:'Wrist hand', leftHand:'Left', rightHand:'Right', panelSize:'HUD panel size', theme:'Theme', language:'Language', themeDark:'Dark', themeLight:'Light',
        langZh:'Chinese', langEn:'English', hardwareSource:'Primary data source', afterburner:'MSI Afterburner', hwinfo:'HWiNFO', interval:'Update interval', apply:'Apply', connect:'Connect SteamVR',
        sensors:'Detected Sensors', sensorHint:'Select exact readings to show in the VR overlay.', filter:'Filter sensors',
        noRows:'No matching sensor data', metric:'Metric', device:'Device', value:'Value', source:'Source', rawLabel:'Raw Label',
        settingsApplied:'Settings applied', steamVrConnected:'SteamVR overlay connected',
        steamVrConnectFailed:'SteamVR is not running or overlay connection failed'
      },
      zh: {
        title:'VR Performance Profiler', overlayHeading:'\u8986\u76D6', appearanceHeading:'\u754C\u9762', dataHeading:'\u6570\u636E',
        hud:'HUD', wrist:'\u624B\u8155', wristHand:'\u624B\u8155\u4F4D\u7F6E', leftHand:'\u5DE6\u624B', rightHand:'\u53F3\u624B', panelSize:'HUD \u9762\u677F\u5927\u5C0F', theme:'\u4E3B\u9898', language:'\u8BED\u8A00', themeDark:'\u6DF1\u8272', themeLight:'\u6D45\u8272',
        langZh:'\u4E2D\u6587', langEn:'English', hardwareSource:'\u4E3B\u6570\u636E\u6765\u6E90', afterburner:'MSI Afterburner', hwinfo:'HWiNFO', interval:'\u66F4\u65B0\u95F4\u9694', apply:'\u5E94\u7528', connect:'\u8FDE\u63A5 SteamVR',
        sensors:'\u68C0\u6D4B\u5230\u7684\u4F20\u611F\u5668', sensorHint:'\u9009\u62E9\u8981\u663E\u793A\u5728 VR \u8986\u76D6\u4E2D\u7684\u5177\u4F53\u8BFB\u6570\u3002', filter:'\u8FC7\u6EE4\u4F20\u611F\u5668',
        noRows:'\u6CA1\u6709\u5339\u914D\u7684\u4F20\u611F\u5668\u6570\u636E', metric:'\u7C7B\u578B', device:'GPU / \u8BBE\u5907', value:'\u6570\u503C', source:'\u6765\u6E90', rawLabel:'\u539F\u59CB\u6807\u7B7E',
        settingsApplied:'\u8BBE\u7F6E\u5DF2\u5E94\u7528', steamVrConnected:'SteamVR \u8986\u76D6\u8FDE\u63A5\u6210\u529F',
        steamVrConnectFailed:'SteamVR \u672A\u542F\u52A8\u6216\u8986\u76D6\u8FDE\u63A5\u5931\u8D25'
      }
    };
    function lang(){ return state.config.language === 'en' ? 'en' : 'zh'; }
    function t(key){ return strings[lang()][key] || key; }
    function post(type){ chrome.webview.postMessage({ type, ...collect() }); }
    function collect(){
      return {
        mode: state.config.mode,
        overlayWidthMeters: Number($('panelSize').value),
        wristHand: state.config.wristHand || 'left',
        theme: $('theme').value,
        language: $('language').value,
        hardwareSource: $('hardwareSource').value,
        updateIntervalMs: Number($('interval').value),
        selectedKeys: Array.from(document.querySelectorAll('tbody input[type=checkbox]:checked')).map(x => x.value)
      };
    }
    function applyTheme(){
      document.body.dataset.theme = state.config.theme === 'light' ? 'light' : 'dark';
    }
    function setText(id, text){ const el=$(id); if (el) el.textContent = text; }
    function renderText(){
      document.documentElement.lang = lang() === 'en' ? 'en' : 'zh-CN';
      setText('title', t('title'));
      setText('overlayHeading', t('overlayHeading'));
      setText('appearanceHeading', t('appearanceHeading'));
      setText('dataHeading', t('dataHeading'));
      setText('modeHud', t('hud'));
      setText('modeWrist', t('wrist'));
      setText('wristHandLabel', t('wristHand'));
      setText('handLeft', t('leftHand'));
      setText('handRight', t('rightHand'));
      setText('panelSizeLabel', t('panelSize'));
      setText('themeLabel', t('theme'));
      setText('languageLabel', t('language'));
      setText('hardwareSourceLabel', t('hardwareSource'));
      setText('intervalLabel', t('interval'));
      setText('apply', t('apply'));
      setText('connect', t('connect'));
      setText('sensorsHeading', t('sensors'));
      setText('sensorHint', t('sensorHint'));
      $('filter').placeholder = t('filter');
      $('theme').querySelector('option[value="dark"]').textContent = t('themeDark');
      $('theme').querySelector('option[value="light"]').textContent = t('themeLight');
      $('language').querySelector('option[value="zh"]').textContent = t('langZh');
      $('language').querySelector('option[value="en"]').textContent = t('langEn');
      $('hardwareSource').querySelector('option[value="afterburner"]').textContent = t('afterburner');
      $('hardwareSource').querySelector('option[value="hwinfo"]').textContent = t('hwinfo');
      renderStatus();
    }
    function renderStatus(){
      const el=$('status');
      el.textContent = state.statusMessage ? t(state.statusMessage) : '';
      el.className='status ' + (state.statusOk===true?'good':state.statusOk===false?'bad':'');
    }
    function setStatus(message, ok){
      state.statusMessage = message || '';
      state.statusOk = ok;
      renderStatus();
    }
    function render(){
      $('modeHud').classList.toggle('active', state.config.mode === 'hud');
      $('modeWrist').classList.toggle('active', state.config.mode === 'wrist');
      $('handLeft').classList.toggle('active', (state.config.wristHand || 'left') !== 'right');
      $('handRight').classList.toggle('active', state.config.wristHand === 'right');
      const panelSize = Math.min(2.5, Math.max(0.5, Number(state.config.overlayWidthMeters || 1.5)));
      $('panelSize').value = panelSize.toFixed(2);
      $('panelSizeValue').textContent = `${panelSize.toFixed(2)} m`;
      $('theme').value = state.config.theme || 'dark';
      $('language').value = state.config.language || 'zh';
      $('hardwareSource').value = state.config.hardwareSource === 'hwinfo' ? 'hwinfo' : 'afterburner';
      $('interval').value = String(state.config.updateIntervalMs || 66);
      applyTheme();
      renderText();
      const selected = new Set(state.config.selectedKeys || []);
      const term = state.filter.toLowerCase();
      const rows = state.readings.filter(r => !term || [r.categoryName,r.device,r.label,r.source,r.value].join(' ').toLowerCase().includes(term));
      if (!rows.length) { $('table').innerHTML = `<div class="empty">${esc(t('noRows'))}</div>`; return; }
      $('table').innerHTML = `<table><thead><tr><th></th><th>${esc(t('metric'))}</th><th>${esc(t('device'))}</th><th>${esc(t('value'))}</th><th>${esc(t('source'))}</th><th>${esc(t('rawLabel'))}</th></tr></thead><tbody>${rows.map(r => `<tr><td><input type="checkbox" value="${esc(r.key)}" ${selected.has(r.key)?'checked':''}></td><td>${esc(r.categoryName)}</td><td>${esc(r.device||'')}</td><td>${esc(r.value)}</td><td>${esc(r.source||'')}</td><td>${esc(r.label||'')}</td></tr>`).join('')}</tbody></table>`;
    }
    function esc(v){ return String(v ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c])); }
    $('modeHud').onclick=()=>{state.config.mode='hud'; render();};
    $('modeWrist').onclick=()=>{state.config.mode='wrist'; render();};
    $('handLeft').onclick=()=>{state.config.wristHand='left'; state.config.mode='wrist'; render();};
    $('handRight').onclick=()=>{state.config.wristHand='right'; state.config.mode='wrist'; render();};
    $('panelSize').oninput=e=>{state.config.overlayWidthMeters=Number(e.target.value); render();};
    $('theme').onchange=e=>{state.config.theme=e.target.value; render(); post('previewTheme');};
    $('language').onchange=e=>{state.config.language=e.target.value; render();};
    $('hardwareSource').onchange=e=>{state.config.hardwareSource=e.target.value; post('refresh'); render();};
    $('apply').onclick=()=>post('apply');
    $('connect').onclick=()=>post('connect');
    $('filter').oninput=e=>{state.filter=e.target.value; render();};
    chrome.webview.addEventListener('message', ev => {
      const msg = ev.data;
      if (msg.type === 'state') {
        const current = state.initialized ? collect() : null;
        state.config = msg.config;
        if (current) {
          state.config.mode = current.mode;
          state.config.overlayWidthMeters = current.overlayWidthMeters;
          state.config.wristHand = current.wristHand;
          state.config.theme = current.theme;
          state.config.language = current.language;
          state.config.hardwareSource = current.hardwareSource;
          state.config.updateIntervalMs = current.updateIntervalMs;
          state.config.selectedKeys = current.selectedKeys;
        }
        state.readings = msg.readings || [];
        state.initialized = true;
        render();
      }
      if (msg.type === 'status') { setStatus(msg.message, msg.ok); }
    });
    render();
    chrome.webview.postMessage({type:'ready'});
  </script>
</body>
</html>
)HTML";
}

} // namespace vrperf

#else

namespace vrperf {

WebSettingsWindow::WebSettingsWindow() = default;
WebSettingsWindow::~WebSettingsWindow() = default;

bool WebSettingsWindow::Show(HWND,
                             Config&,
                             ReadingsProvider,
                             ApplyCallback,
                             ConnectCallback)
{
    return false;
}

} // namespace vrperf

#endif
