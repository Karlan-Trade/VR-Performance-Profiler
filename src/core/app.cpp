#include "core/app.h"

#include <filesystem>
#include <iostream>

namespace vrperf {

App::App() = default;
App::~App() = default;

bool App::Initialize()
{
    // Load config
    if (!config_.Load()) {
        // Config will use defaults if file not found
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
    if (!d2dRenderer_.SetTargetTexture(d3d11Renderer_.GetTexture())) {
        return false;
    }

    // Initialize optional HWiNFO reader (non-fatal if fails)
    if (!hwinfoReader_.Open()) {
        // Will retry periodically
    }

    overlayPositioner_.SetHudPosition(
        config_.hud.yawDegrees,
        config_.hud.pitchDegrees,
        config_.hud.distanceMeters);
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

    TryStartSensorBridge();
    if (config_.overlay.autoConnectVr) {
        TryInitializeOverlay();
    }
    UpdateTrayTooltip();

    // Register hotkeys
    RegisterHotKey(hwnd_, HOTKEY_TOGGLE_VIS, MOD_CONTROL | MOD_SHIFT, 'H');
    RegisterHotKey(hwnd_, HOTKEY_SWITCH_MODE, MOD_CONTROL | MOD_SHIFT, 'M');

    // Start update timer
    SetTimer(hwnd_, timerId_, config_.overlay.updateIntervalMs, nullptr);

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
    StopSensorBridge();
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
            trayIcon_.ShowMenu(hwnd, pt.x, pt.y);
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
            overlayManager_.ToggleVisibility();
            UpdateTrayTooltip();
            return 0;
        case TRAY_MENU_SWITCH_MODE:
            overlayPositioner_.ToggleMode();
            ApplyOverlayTransform();
            return 0;
        case TRAY_MENU_CONNECT_VR:
            TryInitializeOverlay();
            UpdateTrayTooltip();
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

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void App::OnTimer()
{
    if (overlayManager_.IsInitialized()) {
        // Poll OpenVR events
        overlayManager_.PollEvents();

        // Update overlay content
        UpdateOverlay();
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

bool App::TryInitializeOverlay()
{
    lastOverlayRetryMs_ = GetTickCount();

    if (overlayManager_.IsInitialized()) {
        return true;
    }

    if (!overlayManager_.Initialize()) {
        return false;
    }

    if (!overlayManager_.CreateOverlay(
            "vrperf.profiler.overlay",
            "VR Performance Profiler")) {
        overlayManager_.Shutdown();
        return false;
    }

    overlayManager_.SetAlpha(config_.overlay.alpha);
    overlayManager_.SetInputNone();
    ApplyOverlayTransform();

    if (config_.overlay.visibleOnStart) {
        overlayManager_.Show();
    }

    return true;
}

void App::TryStartSensorBridge()
{
    if (sensorBridgeStarted_) {
        return;
    }

    const auto exeDir = GetExecutableDirectory();
    if (exeDir.empty()) {
        return;
    }

    const auto bridgePath = std::filesystem::path(exeDir)
        / L"lhm_bridge"
        / L"VRPerfProfiler.LhmBridge.exe";
    if (!std::filesystem::exists(bridgePath)) {
        return;
    }

    std::wstring commandLine = L"\"" + bridgePath.wstring() + L"\"";
    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);

    if (CreateProcessW(
            bridgePath.c_str(),
            commandLine.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &startupInfo,
            &sensorBridgeProcess_)) {
        sensorBridgeStarted_ = true;
    }
}

void App::StopSensorBridge()
{
    if (!sensorBridgeStarted_) {
        return;
    }

    if (sensorBridgeProcess_.hProcess) {
        TerminateProcess(sensorBridgeProcess_.hProcess, 0);
        WaitForSingleObject(sensorBridgeProcess_.hProcess, 1000);
        CloseHandle(sensorBridgeProcess_.hProcess);
        sensorBridgeProcess_.hProcess = nullptr;
    }

    if (sensorBridgeProcess_.hThread) {
        CloseHandle(sensorBridgeProcess_.hThread);
        sensorBridgeProcess_.hThread = nullptr;
    }

    sensorBridgeStarted_ = false;
}

std::wstring App::GetExecutableDirectory()
{
    wchar_t path[MAX_PATH] = {};
    const auto length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return {};
    }

    return std::filesystem::path(path).parent_path().wstring();
}

void App::OpenSettings()
{
    if (!settingsWindow_.Show(hwnd_, config_)) {
        return;
    }

    overlayPositioner_.SetHudPosition(
        config_.hud.yawDegrees,
        config_.hud.pitchDegrees,
        config_.hud.distanceMeters);
    overlayPositioner_.SetMode(config_.overlay.mode == "wrist"
        ? OverlayMode::Wrist
        : OverlayMode::HUD);
    overlayPositioner_.SetWristHand(config_.wrist.hand != "right");

    if (overlayManager_.IsInitialized()) {
        overlayManager_.SetAlpha(config_.overlay.alpha);
        ApplyOverlayTransform();
    }

    UpdateTrayTooltip();
}

void App::UpdateTrayTooltip()
{
    if (!trayIcon_.IsCreated()) {
        return;
    }

    if (overlayManager_.IsInitialized()) {
        trayIcon_.UpdateTooltip(L"VR Performance Profiler - SteamVR connected");
    } else {
        trayIcon_.UpdateTooltip(L"VR Performance Profiler - overlay disconnected");
    }
}

void App::UpdateOverlay()
{
    metricAggregator_.Clear();

    if (libreHardwareMonitorBridgeProvider_.Refresh()) {
        metricAggregator_.AddReadings(libreHardwareMonitorBridgeProvider_.GetReadings());
    }

    if (!hwinfoReader_.IsConnected()) {
        hwinfoReader_.Open();
    }
    if (hwinfoReader_.Refresh()) {
        metricAggregator_.AddReadings(hwinfoReader_.GetReadings());
    }

    windowsFallbackProvider_.Refresh();
    metricAggregator_.AddReadings(windowsFallbackProvider_.GetReadings());

    auto readings = metricAggregator_.GetReadings();
    auto frameTiming = openVrFrameTiming_.Read();

    // Render to D3D11 texture
    d2dRenderer_.BeginDraw();
    d2dRenderer_.DrawSensorPanel(
        readings,
        config_,
        frameTiming.available ? frameTiming.gpuFrameTimeMs : 0.0,
        frameTiming.droppedFrames);
    d2dRenderer_.EndDraw();

    // Submit texture to OpenVR
    overlayManager_.SetTexture(d3d11Renderer_.GetSRV());
}

} // namespace vrperf
