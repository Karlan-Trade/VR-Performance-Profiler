# VR Performance Profiler

A SteamVR overlay application that displays real-time system hardware monitoring data in VR.

## Features

- **Real-time hardware monitoring** — CPU/GPU usage, temperature, clock speeds, memory usage
- **SteamVR overlay** — Displayed as a HUD (head-locked) or wrist-mounted overlay
- **Default temperature bridge** — Can read CPU/GPU temperatures from a LibreHardwareMonitor-compatible JSON bridge
- **Optional HWiNFO64 integration** — Reads extra sensor data from HWiNFO's shared memory interface when available
- **Customizable** — Choose which metrics to display, overlay position, theme, and more
- **Lightweight** — Minimal performance overhead, runs alongside VR games

## Requirements

- Windows 10/11
- SteamVR
- Visual Studio 2022 or CMake 3.20+
- .NET 8 SDK or later, for building the LibreHardwareMonitor bridge helper

HWiNFO is optional. If it is unavailable, the app still falls back to basic
Windows CPU/RAM metrics. Temperature data is supplied by the bundled
LibreHardwareMonitor bridge helper when available. The helper writes snapshots
to `%LOCALAPPDATA%/VRPerfProfiler/lhm-sensors.json`.

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

### Running

1. Start SteamVR
2. Optional: start HWiNFO64 with shared memory enabled for extra sensors
3. Run `vr_perf_profiler.exe`

## Configuration

The application stores its configuration in `%APPDATA%/VRPerfProfiler/config.json`.

### Overlay Modes

- **HUD Mode** — Fixed in your field of view, like a car dashboard
- **Wrist Mode** — Attached to your controller, like a virtual watch

### Hotkeys

- `Ctrl+Shift+H` — Toggle overlay visibility
- `Ctrl+Shift+M` — Switch between HUD and Wrist mode

## Architecture

```
LibreHardwareMonitor bridge / HWiNFO / Windows APIs
    → Metric aggregation
    → D2D Renderer
    → OpenVR Overlay
```

- **Data Layer** — Merges optional LibreHardwareMonitor, optional HWiNFO, and Windows fallback metrics
- **Renderer** — Direct2D/Direct3D 11 rendering pipeline
- **Overlay** — OpenVR IVROverlay integration
- **UI** — System tray icon and settings dialog

## Anti-Cheat Safety Boundary

VR Performance Profiler runs as a standalone SteamVR overlay application.
It does not inject into games, hook graphics APIs, read or write game process
memory, or install kernel drivers. Overlay content is submitted to SteamVR via
OpenVR `IVROverlay::SetOverlayTexture`.

## License

MIT License
