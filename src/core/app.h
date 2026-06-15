#pragma once

#include "core/types.h"
#include "core/config.h"
#include "data/libre_hardware_monitor_bridge_provider.h"
#include "data/metric_aggregator.h"
#include "data/msi_afterburner_provider.h"
#include "data/windows_fallback_provider.h"
#include "hwinfo/hwinfo_reader.h"
#include "overlay/overlay_manager.h"
#include "overlay/overlay_positioner.h"
#include "render/d3d11_renderer.h"
#include "render/d2d_renderer.h"
#include "ui/tray_icon.h"
#include "vr/openvr_frame_timing.h"

#include <Windows.h>

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
        const OpenVrFrameTimingSnapshot& frameTiming);
    void UpdateOverlay();
    void ApplyOverlayTransform();
    void ApplyRuntimeConfig();
    bool ConnectSteamVrOverlay(HWND ownerHwnd, bool showMessage);
    bool TryInitializeOverlay();
    void TryStartSensorBridge();
    void StopSensorBridge();
    void OpenSettings();
    void UpdateTrayTooltip();
    static std::wstring GetExecutableDirectory();

    // Window & message pump
    HWND hwnd_ = nullptr;
    UINT_PTR timerId_ = 1;
    static constexpr UINT WM_TRAYICON = WM_APP + 1;

    // Hotkey IDs
    static constexpr int HOTKEY_TOGGLE_VIS = 1;
    static constexpr int HOTKEY_SWITCH_MODE = 2;

    // Subsystems
    Config config_;
    HwInfoReader hwinfoReader_;
    MsiAfterburnerProvider msiAfterburnerProvider_;
    LibreHardwareMonitorBridgeProvider libreHardwareMonitorBridgeProvider_;
    WindowsFallbackProvider windowsFallbackProvider_;
    MetricAggregator metricAggregator_;
    OverlayManager overlayManager_;
    OverlayPositioner overlayPositioner_;
    D3D11Renderer d3d11Renderer_;
    D2DRenderer d2dRenderer_;
    OpenVrFrameTiming openVrFrameTiming_;
    TrayIcon trayIcon_;

    bool running_ = false;
    DWORD lastOverlayRetryMs_ = 0;
    PROCESS_INFORMATION sensorBridgeProcess_ = {};
    bool sensorBridgeStarted_ = false;
};

} // namespace vrperf
