#include "core/app.h"

#include "core/log.h"
#include "ui/web_settings_window.h"

#include <algorithm>
#include <atomic>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <thread>
#include <TlHelp32.h>

namespace vrperf {

namespace {
constexpr DWORD kSteamVrConnectTimeoutMs = 15000;

bool HasExactMetricSelection(const Config& config)
{
    return std::any_of(
        config.metrics.begin(),
        config.metrics.end(),
        [](const MetricConfig& metric) {
            return metric.enabled && !metric.sensorKey.empty();
        });
}

bool MetricMatchesReading(const MetricConfig& metric,
                          const SensorReading& reading,
                          bool exactOnly)
{
    if (!metric.enabled) {
        return false;
    }

    if (!metric.sensorKey.empty()) {
        return metric.sensorKey == SensorReadingKey(reading) ||
               metric.sensorKey == LegacySensorReadingKey(reading);
    }

    return !exactOnly && metric.category == SensorCategoryKey(reading.category);
}

std::vector<SensorReading> SelectConfiguredReadings(
    const std::vector<SensorReading>& detectedReadings,
    const Config& config)
{
    std::vector<SensorReading> selected;
    const bool exactOnly = HasExactMetricSelection(config);

    for (const auto& metric : config.metrics) {
        for (const auto& reading : detectedReadings) {
            if (!MetricMatchesReading(metric, reading, exactOnly)) {
                continue;
            }

            auto display = reading;
            if (!metric.label.empty()) {
                display.label = metric.label;
            }
            selected.push_back(std::move(display));
            if (exactOnly) {
                break;
            }
        }
    }

    return selected;
}

bool IsProcessRunning(const wchar_t* processName)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    bool found = false;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, processName) == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return found;
}

bool IsSteamVrRunning()
{
    return IsProcessRunning(L"vrserver.exe");
}

std::wstring ExecutableDirectory()
{
    std::wstring path(MAX_PATH, L'\0');
    DWORD size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    while (size == path.size() && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        path.resize(path.size() * 2);
        size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    }
    if (size == 0) {
        return L".";
    }

    path.resize(size);
    const auto slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : path.substr(0, slash);
}

std::string NarrowForLog(const std::wstring& text)
{
    if (text.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return "<wide string conversion failed>";
    }

    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

bool RunSteamVrInitProbe(DWORD timeoutMs)
{
    const auto exeDir = ExecutableDirectory();
    wchar_t exePathBuffer[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, exePathBuffer, MAX_PATH) == 0) {
        LogInfo("App SteamVR readiness probe: failed to resolve executable path");
        return false;
    }
    const std::wstring exePath = exePathBuffer;

    std::wstring commandLine = L"\"" + exePath + L"\" --steamvr-init-probe";
    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo = {};

    LogInfo("App SteamVR readiness probe: start");
    if (!CreateProcessW(
            exePath.c_str(),
            commandLine.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            exeDir.c_str(),
            &startupInfo,
            &processInfo)) {
        std::ostringstream ss;
        ss << "App SteamVR readiness probe: CreateProcess failed error="
           << GetLastError();
        LogInfo(ss.str());
        return false;
    }

    const DWORD waitResult = WaitForSingleObject(processInfo.hProcess, timeoutMs);
    bool ok = false;
    if (waitResult == WAIT_OBJECT_0) {
        DWORD exitCode = 1;
        GetExitCodeProcess(processInfo.hProcess, &exitCode);
        ok = exitCode == 0;
        std::ostringstream ss;
        ss << "App SteamVR readiness probe: exit=" << exitCode;
        LogInfo(ss.str());
    } else if (waitResult == WAIT_TIMEOUT) {
        LogInfo("App SteamVR readiness probe: timeout waiting for VR_Init");
        TerminateProcess(processInfo.hProcess, 2);
        WaitForSingleObject(processInfo.hProcess, 3000);
    } else {
        std::ostringstream ss;
        ss << "App SteamVR readiness probe: wait failed result="
           << waitResult << " error=" << GetLastError();
        LogInfo(ss.str());
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return ok;
}

ColorTheme ThemeFromName(const std::string& theme)
{
    return theme == "light" ? LightTheme() : DarkTheme();
}

std::wstring AppText(const std::string& language, const wchar_t* zh, const wchar_t* en)
{
    return language == "en" ? en : zh;
}

SensorReading MakeVrReading(SensorCategory category,
                            const char* label,
                            double value,
                            const char* unit,
                            int readingId)
{
    SensorReading reading;
    reading.category = category;
    reading.source = "SteamVR";
    reading.device = "SteamVR Compositor";
    reading.label = label;
    reading.value = value;
    reading.unit = unit;
    reading.sensorId = 9000;
    reading.readingId = readingId;
    return reading;
}

void AppendVrFrameReadings(std::vector<SensorReading>& readings,
                           const OpenVrFrameTimingSnapshot& frameTiming)
{
    const double unavailable = std::numeric_limits<double>::quiet_NaN();

    const double fps = frameTiming.smoothedFps > 0.0
        ? frameTiming.smoothedFps
        : frameTiming.fps;
    readings.push_back(MakeVrReading(
        SensorCategory::VrFps,
        "VR FPS",
        frameTiming.available && fps > 0.0 ? fps : unavailable,
        "FPS",
        1));

    readings.push_back(MakeVrReading(
        SensorCategory::VrFrameTime,
        "VR frame interval",
        frameTiming.available && frameTiming.frameTimeMs > 0.0
            ? frameTiming.frameTimeMs
            : unavailable,
        "ms",
        2));

    readings.push_back(MakeVrReading(
        SensorCategory::VrGpuFrameTime,
        "VR GPU frame time",
        frameTiming.available && frameTiming.gpuFrameTimeMs > 0.0
            ? frameTiming.gpuFrameTimeMs
            : unavailable,
        "ms",
        3));

    readings.push_back(MakeVrReading(
        SensorCategory::VrRefreshRate,
        "Headset refresh rate",
        frameTiming.available && frameTiming.refreshHz > 0.0
            ? frameTiming.refreshHz
            : unavailable,
        "Hz",
        4));

    readings.push_back(MakeVrReading(
        SensorCategory::VrDroppedFrames,
        "Dropped frames",
        frameTiming.available
            ? static_cast<double>(frameTiming.droppedFrames)
            : unavailable,
        "",
        5));
}

} // namespace

App::App() = default;
App::~App() = default;

bool App::Initialize()
{
    LogInfo("App Initialize: start");
    // Load config
    if (!config_.Load()) {
        // Config will use defaults if file not found
    }
    {
        std::ostringstream ss;
        ss << "App config: version=" << config_.version
           << " autoConnectVr=" << config_.overlay.autoConnectVr
           << " visibleOnStart=" << config_.overlay.visibleOnStart
           << " mode=" << config_.overlay.mode
           << " hardwareSource=" << config_.data.hardwareSource
           << " width=" << config_.overlay.widthMeters
           << " hudDistance=" << config_.hud.distanceMeters
           << " hudPitch=" << config_.hud.pitchDegrees
           << " offset=(" << config_.overlay.offsetX << ","
           << config_.overlay.offsetY << ","
           << config_.overlay.offsetZ << ")";
        LogInfo(ss.str());
    }

    // Initialize D3D11 renderer
    if (!d3d11Renderer_.Initialize(
            static_cast<uint32_t>(config_.appearance.textureWidth),
            static_cast<uint32_t>(config_.appearance.textureHeight))) {
        return false;
    }

    // Initialize D2D renderer
    if (!d2dRenderer_.Initialize(d3d11Renderer_.GetDevice())) {
        return false;
    }
    d2dRenderer_.SetTheme(ThemeFromName(config_.appearance.theme));
    if (!d2dRenderer_.SetTargetTexture(d3d11Renderer_.GetTexture())) {
        return false;
    }

    overlayPositioner_.SetHudPosition(
        config_.hud.yawDegrees,
        config_.hud.pitchDegrees,
        config_.hud.distanceMeters);
    overlayPositioner_.SetOverlayOffset(
        config_.overlay.offsetX,
        config_.overlay.offsetY,
        config_.overlay.offsetZ);
    overlayPositioner_.SetWristOffset(
        config_.wrist.offsetX,
        config_.wrist.offsetY,
        config_.wrist.offsetZ);
    overlayPositioner_.SetWristOffsetScale(config_.wrist.offsetScale);
    overlayPositioner_.SetMode(config_.overlay.mode == "wrist"
        ? OverlayMode::Wrist
        : OverlayMode::HUD);
    overlayPositioner_.SetWristHand(config_.wrist.hand != "right");

    // Create message-only window
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"VRPerfProfilerMsgWnd";
    RegisterClassEx(&wc);

    hwnd_ = CreateWindowEx(
        0, wc.lpszClassName, L"VR Perf Profiler",
        0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, wc.hInstance, this
    );

    if (!hwnd_) {
        return false;
    }

    // Store this pointer for WndProc
    SetWindowLongPtr(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    if (!trayIcon_.Create(hwnd_, GetModuleHandle(nullptr), WM_TRAYICON)) {
        return false;
    }

    UpdateTrayTooltip();

    // Register hotkeys
    RegisterHotKey(hwnd_, HOTKEY_TOGGLE_VIS, MOD_CONTROL | MOD_SHIFT, 'H');
    RegisterHotKey(hwnd_, HOTKEY_SWITCH_MODE, MOD_CONTROL | MOD_SHIFT, 'M');

    // Start update timer
    SetTimer(hwnd_, timerId_, config_.overlay.updateIntervalMs, nullptr);
    PostMessage(hwnd_, WM_COMMAND, TRAY_MENU_SETTINGS, 0);

    running_ = true;
    return true;
}

void App::Run()
{
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void App::Shutdown()
{
    running_ = false;

    if (hwnd_) {
        KillTimer(hwnd_, timerId_);
        UnregisterHotKey(hwnd_, HOTKEY_TOGGLE_VIS);
        UnregisterHotKey(hwnd_, HOTKEY_SWITCH_MODE);
        trayIcon_.Destroy();
        DestroyWindow(hwnd_);
    }

    overlayManager_.Shutdown();
    hwinfoReader_.Close();
    d2dRenderer_.Shutdown();
    d3d11Renderer_.Shutdown();
}

LRESULT CALLBACK App::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    App* app = reinterpret_cast<App*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (app) {
        return app->HandleMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT App::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_TRAYICON:
        if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU) {
            POINT pt;
            GetCursorPos(&pt);
            trayIcon_.ShowMenu(hwnd, pt.x, pt.y, config_.general.language);
        } else if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
            OpenSettings();
        }
        return 0;

    case WM_TIMER:
        if (wParam == timerId_) {
            OnTimer();
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case TRAY_MENU_TOGGLE_VIS:
            if (!overlayManager_.IsInitialized()) {
                UpdateTrayTooltip();
                return 0;
            }
            overlayManager_.ToggleVisibility();
            UpdateTrayTooltip();
            return 0;
        case TRAY_MENU_SWITCH_MODE:
            overlayPositioner_.ToggleMode();
            ApplyOverlayTransform();
            return 0;
        case TRAY_MENU_CONNECT_VR:
            ConnectSteamVrOverlayAsync([this](bool connected) {
                HWND messageOwner = IsWindowVisible(hwnd_) ? hwnd_ : nullptr;
                MessageBoxW(
                    messageOwner,
                    connected
                        ? AppText(config_.general.language,
                                  L"SteamVR \u8986\u76D6\u8FDE\u63A5\u6210\u529F.",
                                  L"SteamVR overlay connected.").c_str()
                        : AppText(config_.general.language,
                                  L"SteamVR \u8986\u76D6\u8FDE\u63A5\u5931\u8D25.\u8BF7\u5148\u542F\u52A8 SteamVR\uFF0C\u7136\u540E\u91CD\u8BD5.",
                                  L"SteamVR overlay connection failed. Start SteamVR first, then retry.").c_str(),
                    AppText(config_.general.language,
                            L"SteamVR \u8FDE\u63A5",
                            L"SteamVR Connection").c_str(),
                    connected ? MB_OK | MB_ICONINFORMATION : MB_OK | MB_ICONWARNING);
            });
            return 0;
        case TRAY_MENU_SETTINGS:
            OpenSettings();
            return 0;
        case TRAY_MENU_EXIT:
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_HOTKEY:
        OnHotkey(static_cast<int>(wParam));
        return 0;

    case WM_STEAMVR_INIT_DONE: {
        if (!connectingSteamVr_.load()) {
            steamVrInitThreadActive_.store(false);
            return 0;
        }
        bool connected = wParam != 0;
        if (connected) {
            connected = TryInitializeOverlay();
        }
        connectingSteamVr_.store(false);
        steamVrInitThreadActive_.store(false);
        connectingSteamVrStartedMs_ = 0;
        UpdateTrayTooltip();
        if (pendingSteamVrConnectCompletion_) {
            auto completion = std::move(pendingSteamVrConnectCompletion_);
            pendingSteamVrConnectCompletion_ = nullptr;
            completion(connected);
        }
        return 0;
    }

    case WM_DESTROY:
        if (hwnd == hwnd_) {
            KillTimer(hwnd_, timerId_);
            UnregisterHotKey(hwnd_, HOTKEY_TOGGLE_VIS);
            UnregisterHotKey(hwnd_, HOTKEY_SWITCH_MODE);
            trayIcon_.Destroy();
            hwnd_ = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void App::OnTimer()
{
    if (connectingSteamVr_.load()) {
        const DWORD started = connectingSteamVrStartedMs_;
        if (started != 0 && GetTickCount() - started >= kSteamVrConnectTimeoutMs) {
            connectingSteamVr_.store(false);
            connectingSteamVrStartedMs_ = 0;
            LogInfo("App async SteamVR connect: timeout");
            if (pendingSteamVrConnectCompletion_) {
                auto completion = std::move(pendingSteamVrConnectCompletion_);
                pendingSteamVrConnectCompletion_ = nullptr;
                completion(false);
            }
        }
    }

    if (overlayManager_.IsInitialized()) {
        // Poll OpenVR events
        overlayManager_.PollEvents();

        // Update overlay content
        if (overlayManager_.IsOverlayVisible()) {
            UpdateOverlay();
        }
    }
}

void App::OnHotkey(int id)
{
    switch (id) {
    case HOTKEY_TOGGLE_VIS:
        overlayManager_.ToggleVisibility();
        break;
    case HOTKEY_SWITCH_MODE:
        overlayPositioner_.ToggleMode();
        ApplyOverlayTransform();
        break;
    }
}

void App::ApplyOverlayTransform()
{
    if (!overlayManager_.IsInitialized()) {
        return;
    }

    overlayPositioner_.ApplyTransform(&overlayManager_);
    overlayManager_.SetWidthMeters(
        overlayPositioner_.GetMode() == OverlayMode::Wrist
            ? config_.wrist.widthMeters
            : config_.overlay.widthMeters);
}

void App::ApplyRuntimeConfig()
{
    d2dRenderer_.SetTheme(ThemeFromName(config_.appearance.theme));

    overlayPositioner_.SetHudPosition(
        config_.hud.yawDegrees,
        config_.hud.pitchDegrees,
        config_.hud.distanceMeters);
    overlayPositioner_.SetOverlayOffset(
        config_.overlay.offsetX,
        config_.overlay.offsetY,
        config_.overlay.offsetZ);
    overlayPositioner_.SetWristOffset(
        config_.wrist.offsetX,
        config_.wrist.offsetY,
        config_.wrist.offsetZ);
    overlayPositioner_.SetWristOffsetScale(config_.wrist.offsetScale);
    overlayPositioner_.SetMode(config_.overlay.mode == "wrist"
        ? OverlayMode::Wrist
        : OverlayMode::HUD);
    overlayPositioner_.SetWristHand(config_.wrist.hand != "right");

    if (overlayManager_.IsInitialized()) {
        overlayManager_.SetAlpha(config_.overlay.alpha);
        ApplyOverlayTransform();
        UpdateOverlay();
    }

    KillTimer(hwnd_, timerId_);
    SetTimer(hwnd_, timerId_, config_.overlay.updateIntervalMs, nullptr);
    UpdateTrayTooltip();
}

bool App::ConnectSteamVrOverlay(HWND ownerHwnd, bool showMessage)
{
    const bool connected = TryInitializeOverlay();
    UpdateTrayTooltip();

    if (showMessage) {
        HWND messageOwner = IsWindowVisible(ownerHwnd) ? ownerHwnd : nullptr;
        MessageBoxW(
            messageOwner,
            connected
                ? AppText(config_.general.language,
                          L"SteamVR \u8986\u76D6\u8FDE\u63A5\u6210\u529F.",
                          L"SteamVR overlay connected.").c_str()
                : AppText(config_.general.language,
                          L"SteamVR \u8986\u76D6\u8FDE\u63A5\u5931\u8D25.\u8BF7\u5148\u542F\u52A8 SteamVR\uFF0C\u7136\u540E\u91CD\u8BD5.",
                          L"SteamVR overlay connection failed. Start SteamVR first, then retry.").c_str(),
            AppText(config_.general.language,
                    L"SteamVR \u8FDE\u63A5",
                    L"SteamVR Connection").c_str(),
            connected ? MB_OK | MB_ICONINFORMATION : MB_OK | MB_ICONWARNING);
    }

    return connected;
}

void App::ConnectSteamVrOverlayAsync(std::function<void(bool)> completion)
{
    if (connectingSteamVr_.exchange(true) || steamVrInitThreadActive_.load()) {
        if (completion) {
            completion(false);
        }
        return;
    }

    pendingSteamVrConnectCompletion_ = std::move(completion);
    connectingSteamVrStartedMs_ = GetTickCount();

    if (overlayManager_.IsInitialized()) {
        PostMessage(hwnd_, WM_STEAMVR_INIT_DONE, 1, 0);
        return;
    }

    if (!IsSteamVrRunning()) {
        LogInfo("App async SteamVR connect: vrserver.exe is not running");
        PostMessage(hwnd_, WM_STEAMVR_INIT_DONE, 0, 0);
        return;
    }

    HWND resultHwnd = hwnd_;
    steamVrInitThreadActive_.store(true);
    std::thread([resultHwnd]() {
        const bool ready = RunSteamVrInitProbe(kSteamVrConnectTimeoutMs);
        if (resultHwnd && IsWindow(resultHwnd)) {
            PostMessage(resultHwnd, WM_STEAMVR_INIT_DONE, ready ? 1 : 0, 0);
        }
    }).detach();
}

bool App::TryInitializeOverlay()
{
    lastOverlayRetryMs_ = GetTickCount();
    LogInfo("App TryInitializeOverlay: start");

    if (overlayManager_.IsInitialized()) {
        LogInfo("App TryInitializeOverlay: already initialized");
        return true;
    }

    if (!IsSteamVrRunning()) {
        LogInfo("App TryInitializeOverlay: SteamVR vrserver.exe is not running");
        return false;
    }

    if (!overlayManager_.Initialize()) {
        LogInfo("App TryInitializeOverlay: OverlayManager Initialize failed");
        return false;
    }

    return CompleteInitializedOverlay();
}

bool App::CompleteInitializedOverlay()
{
    if (overlayManager_.IsInitialized() && overlayManager_.GetHandle() != 0) {
        overlayManager_.SetAlpha(config_.overlay.alpha);
        ApplyOverlayTransform();
        UpdateOverlay();
        if (config_.overlay.visibleOnStart) {
            overlayManager_.Show();
        }
        LogInfo("App CompleteInitializedOverlay: already initialized");
        return true;
    }

    if (!overlayManager_.IsInitialized() &&
        !overlayManager_.AttachInitializedSession()) {
        LogInfo("App CompleteInitializedOverlay: attach failed");
        return false;
    }

    if (!overlayManager_.CreateOverlay(
            "vrperf.profiler.overlay",
            "VR Performance Profiler")) {
        overlayManager_.Shutdown();
        LogInfo("App CompleteInitializedOverlay: CreateOverlay failed");
        return false;
    }

    overlayManager_.SetAlpha(config_.overlay.alpha);
    overlayManager_.SetInputNone();
    ApplyOverlayTransform();
    UpdateOverlay();

    if (config_.overlay.visibleOnStart) {
        overlayManager_.Show();
    }

    LogInfo("App CompleteInitializedOverlay: success");
    return true;
}

void App::OpenSettings()
{
    WebSettingsWindow webSettingsWindow;
    const bool openedWebSettings = webSettingsWindow.Show(
        hwnd_,
        config_,
        [this](const std::string& hardwareSource) {
            return CollectDetectedSensorReadings(hardwareSource);
        },
        [this]() {
            ApplyRuntimeConfig();
        },
        [this](std::function<void(bool)> completion) {
            ConnectSteamVrOverlayAsync(std::move(completion));
        });
    if (openedWebSettings) {
        return;
    }

    MessageBoxW(
        hwnd_,
        AppText(config_.general.language,
                L"\u65E0\u6CD5\u6253\u5F00 Web \u8BBE\u7F6E\u754C\u9762\u3002\u8BF7\u786E\u8BA4 WebView2 Runtime \u5DF2\u5B89\u88C5\uFF0C\u6216\u91CD\u65B0\u6784\u5EFA\u5E26 WebView2 \u652F\u6301\u7684\u7248\u672C\u3002",
                L"Unable to open the Web settings UI. Confirm that WebView2 Runtime is installed, or rebuild with WebView2 support.").c_str(),
        AppText(config_.general.language,
                L"\u8BBE\u7F6E\u754C\u9762\u4E0D\u53EF\u7528",
                L"Settings UI unavailable").c_str(),
        MB_OK | MB_ICONERROR);
}

void App::UpdateTrayTooltip()
{
    if (!trayIcon_.IsCreated()) {
        return;
    }

    if (overlayManager_.IsInitialized()) {
        trayIcon_.UpdateTooltip(AppText(
            config_.general.language,
            L"VR Performance Profiler - SteamVR \u5DF2\u8FDE\u63A5",
            L"VR Performance Profiler - SteamVR connected"));
    } else {
        trayIcon_.UpdateTooltip(AppText(
            config_.general.language,
            L"VR Performance Profiler - SteamVR \u672A\u8FDE\u63A5",
            L"VR Performance Profiler - SteamVR disconnected"));
    }
}

void App::UpdateOverlay()
{
    auto frameTiming = openVrFrameTiming_.Read();
    auto readings = SelectConfiguredReadings(
        CollectDetectedSensorReadings(frameTiming),
        config_);

    // Render to D3D11 texture
    d2dRenderer_.BeginDraw();
    d2dRenderer_.DrawSensorPanel(
        readings,
        config_);
    d2dRenderer_.EndDraw();
    if (auto* context = d3d11Renderer_.GetContext()) {
        context->Flush();
    }

    // Submit texture to OpenVR
    const bool textureOk = overlayManager_.SetTexture(d3d11Renderer_.GetTexture());
    if (!textureOk) {
        LogInfo("App UpdateOverlay: SetTexture failed");
    }
}

std::vector<SensorReading> App::CollectSensorReadings()
{
    auto frameTiming = openVrFrameTiming_.Read();
    return SelectConfiguredReadings(
        CollectDetectedSensorReadings(frameTiming),
        config_);
}

std::vector<SensorReading> App::CollectDetectedSensorReadings()
{
    return CollectDetectedSensorReadings(config_.data.hardwareSource);
}

std::vector<SensorReading> App::CollectDetectedSensorReadings(
    const std::string& hardwareSource)
{
    auto frameTiming = overlayManager_.IsInitialized()
        ? openVrFrameTiming_.Read()
        : OpenVrFrameTimingSnapshot{};
    return CollectDetectedSensorReadings(frameTiming, hardwareSource);
}

std::vector<SensorReading> App::CollectDetectedSensorReadings(
    const OpenVrFrameTimingSnapshot& frameTiming)
{
    return CollectDetectedSensorReadings(frameTiming, config_.data.hardwareSource);
}

std::vector<SensorReading> App::CollectDetectedSensorReadings(
    const OpenVrFrameTimingSnapshot& frameTiming,
    const std::string& hardwareSource)
{
    std::vector<SensorReading> readings;

    if (hardwareSource == "hwinfo") {
        if (!hwinfoReader_.IsConnected()) {
            hwinfoReader_.Open();
        }
        if (hwinfoReader_.IsConnected()) {
            hwinfoReader_.Refresh();
            readings = hwinfoReader_.GetReadings();
        }
    } else if (msiAfterburnerProvider_.Refresh()) {
        readings = msiAfterburnerProvider_.GetReadings();
    }

    AppendVrFrameReadings(readings, frameTiming);
    return readings;
}

} // namespace vrperf
