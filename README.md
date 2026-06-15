# VR Performance Profiler

[中文说明](README.zh-CN.md)

A SteamVR overlay application that displays real-time system hardware monitoring data in VR.

## Features

- **Real-time hardware monitoring** — CPU/GPU usage, temperature, clock speeds, memory usage
- **VR frame-rate monitoring** — FPS, frame interval, GPU frame time, headset refresh rate, and dropped frames from SteamVR compositor timing
- **SteamVR overlay** — Displayed as a HUD (head-locked) or wrist-mounted overlay
- **Default MSI Afterburner source** — Uses MSI Afterburner's hardware monitoring shared memory when available
- **Default temperature bridge** — Can read CPU/GPU temperatures from a LibreHardwareMonitor-compatible JSON bridge
- **Optional HWiNFO64 integration** — Reads extra sensor data from HWiNFO's shared memory interface when available
- **Customizable** — Choose which metrics to display, overlay position, theme, and more
- **Lightweight** — Minimal performance overhead, runs alongside VR games

## Requirements

- Windows 10/11
- SteamVR
- Visual Studio 2022 or CMake 3.20+
- .NET 8 SDK or later, for building the LibreHardwareMonitor bridge helper

MSI Afterburner is the preferred default data source when its hardware
monitoring shared memory is available. The app uses the first available source
in this order: MSI Afterburner, LibreHardwareMonitor bridge, HWiNFO, then basic
Windows CPU/RAM metrics. It does not merge sources to fill missing fields.
The LibreHardwareMonitor helper writes snapshots to
`%LOCALAPPDATA%/VRPerfProfiler/lhm-sensors.json`.

## Building

### Prerequisites

1. Install [CMake](https://cmake.org/) 3.20 or later
2. Install Visual Studio 2022 with C++ Desktop Development workload
3. Ensure SteamVR is installed
4. Install .NET 8 SDK or later

### Build Steps

```bash
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

## Packaging

The Windows installer does not bundle SteamVR. It packages the profiler,
OpenVR client DLL, WebView2 loader, and a self-contained x64
LibreHardwareMonitor bridge so users do not need to install the .NET runtime.

Run from a Visual Studio x64 Developer Command Prompt:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\package_installer.ps1
```

Outputs are written to `dist/`:

- `VRPerformanceProfiler-<version>-Setup.exe` installs for the current user
  and lets the user choose the installation location. The default is
  `%LOCALAPPDATA%\Programs\VR Performance Profiler`; if the selected folder is
  not already named `VR Performance Profiler`, the installer creates that
  product subfolder under the selected location.
- `VRPerformanceProfiler-<version>-portable.zip` is a portable fallback.

The installer creates Start Menu and desktop shortcuts, registers an uninstall
entry under the current user, and launches the app after installation. SteamVR
must still be installed separately.

### Running

1. Start SteamVR
2. Start MSI Afterburner for the preferred hardware monitoring source
3. Optional: start HWiNFO64 with shared memory enabled for extra sensors
4. Run `vr_perf_profiler.exe`

The app connects to SteamVR automatically by default. It starts in the system
tray, opens the settings window, and keeps retrying if SteamVR starts later.
The tray menu item `Connect SteamVR Overlay` is still available for an explicit
manual retry.

## Frame Rate Detection

Frame-rate metrics are read through OpenVR's compositor timing API while the app
is connected to SteamVR. They appear in the same detected-sensors table as the
hardware readings, where each VR row can be checked or unchecked before applying
settings. Available rows include smoothed FPS, frame interval, optional
compositor GPU frame time, headset refresh rate, and dropped-frame count.

This is an external SteamVR runtime measurement. It does not inject into the VR
game, hook DirectX/OpenGL/Vulkan, or read game memory. As a result, it is safer
for anti-cheat boundaries, but it may differ from a game's own internal FPS
counter or engine profiler.

## Configuration

The application stores its configuration in `%APPDATA%/VRPerfProfiler/config.json`.
The settings window shows each detected sensor row with source, GPU/device,
raw label, value, and unit. Checked rows are saved as exact sensor selections,
so multi-GPU systems can choose GPU1/GPU2 readings independently.

### Overlay Modes

- **HUD Mode** — Fixed in your field of view, like a car dashboard
- **Wrist Mode** — Attached to your controller, like a virtual watch

### Hotkeys

- `Ctrl+Shift+H` — Toggle overlay visibility
- `Ctrl+Shift+M` — Switch between HUD and Wrist mode

Tray menu:

- `Connect SteamVR Overlay` — Initialize the OpenVR overlay on demand
- `Settings...` — Open the configuration dialog without requiring SteamVR

## Architecture

```
MSI Afterburner → LibreHardwareMonitor bridge → HWiNFO → Windows APIs
SteamVR compositor timing → FPS / frame interval / dropped frames
    → Exact sensor selection + VR frame metrics
    → D2D Renderer
    → OpenVR Overlay
```

- **Data Layer** — Reads the first available hardware provider in priority order, keeps raw sensor rows selectable, and reads VR frame timing from SteamVR when connected
- **Renderer** — Direct2D/Direct3D 11 rendering pipeline
- **Overlay** — OpenVR IVROverlay integration
- **UI** — System tray icon and settings dialog

## Anti-Cheat Safety Boundary

VR Performance Profiler runs as a standalone SteamVR overlay application.
It does not inject into games, hook graphics APIs, read or write game process
memory, or install kernel drivers. Overlay content is submitted to SteamVR via
OpenVR `IVROverlay::SetOverlayTexture`, and frame-rate metrics are read via the
OpenVR compositor timing API.

## License

MIT License. See `LICENSE`.

Third-party dependency notices are listed in `THIRD_PARTY_NOTICES.md`.
