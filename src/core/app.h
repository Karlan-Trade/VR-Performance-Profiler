#pragma once

#include "core/types.h"
#include "core/config.h"
#include "data/metric_aggregator.h"
#include "data/msi_afterburner_provider.h"
#include "hwinfo/hwinfo_reader.h"
#include "overlay/overlay_manager.h"
#include "overlay/overlay_positioner.h"
#include "render/d3d11_renderer.h"
#include "render/d2d_renderer.h"
#include "ui/tray_icon.h"
#include "vr/openvr_frame_timing.h"

#include <Windows.h>
#include <atomic>
#include <functional>

namespace vrperf {

class App {
public:
    App();
    ~App();

    bool Initialize();
    void Run();
    void Shutdown();

private:
    // Message handling
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnTimer();
    void OnHotkey(int id);

    // Update cycle
    std::vector<SensorReading> CollectSensorReadings();
    std::vector<SensorReading> CollectDetectedSensorReadings();
    std::vector<SensorReading> CollectDetectedSensorReadings(
        const std::string& hardwareSource);
    std::vector<SensorReading> CollectDetectedSensorReadings(
        const OpenVrFrameTimingSnapshot& frameTiming);
    std::vector<SensorReading> CollectDetectedSensorReadings(
        const OpenVrFrameTimingSnapshot& frameTiming,
        const std::string& hardwareSource);
    void UpdateOverlay();
    void ApplyOverlayTransform();
    void ApplyRuntimeConfig();
    bool ConnectSteamVrOverlay(HWND ownerHwnd, bool showMessage);
    void ConnectSteamVrOverlayAsync(std::function<void(bool)> completion);
    bool TryInitializeOverlay();
    bool CompleteInitializedOverlay();
    void OpenSettings();
    void UpdateTrayTooltip();

    // Window & message pump
    HWND hwnd_ = nullptr;
    UINT_PTR timerId_ = 1;
    static constexpr UINT WM_TRAYICON = WM_APP + 1;
    static constexpr UINT WM_STEAMVR_INIT_DONE = WM_APP + 2;

    // Hotkey IDs
    static constexpr int HOTKEY_TOGGLE_VIS = 1;
    static constexpr int HOTKEY_SWITCH_MODE = 2;

    // Subsystems
    Config config_;
    MsiAfterburnerProvider msiAfterburnerProvider_;
    HwInfoReader hwinfoReader_;
    MetricAggregator metricAggregator_;
    OverlayManager overlayManager_;
    OverlayPositioner overlayPositioner_;
    D3D11Renderer d3d11Renderer_;
    D2DRenderer d2dRenderer_;
    OpenVrFrameTiming openVrFrameTiming_;
    TrayIcon trayIcon_;

    bool running_ = false;
    std::atomic_bool connectingSteamVr_ = false;
    std::atomic_bool steamVrInitThreadActive_ = false;
    DWORD connectingSteamVrStartedMs_ = 0;
    std::function<void(bool)> pendingSteamVrConnectCompletion_;
    DWORD lastOverlayRetryMs_ = 0;
};

} // namespace vrperf
