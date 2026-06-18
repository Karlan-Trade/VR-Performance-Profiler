# VR Performance Profiler

[中文说明](README.md)

A SteamVR overlay application that displays real-time system hardware monitoring data in VR.

## Features

- **Real-time hardware monitoring** — CPU/GPU usage, temperature, clock speeds, memory usage
- **VR frame-rate monitoring** — FPS, frame interval, GPU frame time, headset refresh rate, and dropped frames from SteamVR compositor timing
- **SteamVR overlay** — Displayed as a HUD (head-locked) or wrist-mounted overlay
- **Selectable hardware source** — Defaults to MSI Afterburner shared memory and can switch to HWiNFO shared memory in settings
- **Customizable** — Choose exact sensor rows, overlay mode, HUD panel size, theme, and update interval
- **Display polish** — Temperature values use the correct `°C` symbol, the HUD fills its SteamVR overlay bounds, and wrist mode defaults to a more compact size
- **Theme-aware settings window** — The Web settings content and native title bar follow the selected light/dark theme
- **Lightweight** — Minimal performance overhead, runs alongside VR games

## Requirements

- Windows 10/11
- SteamVR
- Visual Studio 2022 or CMake 3.20+

MSI Afterburner is the default hardware data source. HWiNFO is available as an
optional primary hardware source from the settings window. If the selected
source's shared memory is unavailable, the app can still display SteamVR
compositor frame metrics, but hardware rows will be unavailable.

## Building

### Prerequisites

1. Install [CMake](https://cmake.org/) 3.20 or later
2. Install Visual Studio 2022 with C++ Desktop Development workload
3. Ensure SteamVR is installed
4. Install WiX Toolset 4 if you want to build the MSI installer

### Build Steps

```bash
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

## Packaging

The Windows installer does not bundle SteamVR. It packages the profiler,
OpenVR client DLL, and WebView2 loader.

Run from a Visual Studio x64 Developer Command Prompt:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\package_installer.ps1
```

Outputs are written to `dist/`:

- `VRPerformanceProfiler-<version>-Setup.msi` installs for the current user
  and lets the user choose the installation location. The default is
  `%LOCALAPPDATA%\Programs\VR Performance Profiler`.
- `VRPerformanceProfiler-<version>-portable.zip` is a portable fallback.

The installer creates a Start Menu shortcut and registers an uninstall entry
under the current user. SteamVR must still be installed separately.

The installer is generated as a standard Windows Installer MSI by WiX Toolset 4.
`wix.exe` must be in PATH or installed at `.tools\wix\wix.exe`. Use `-SkipMsi`
to build only the portable ZIP. The MSI keeps the custom install directory UI,
but app files are copied by an embedded native custom action instead of the
Windows Installer `File` table to avoid `Config.Msi` permission failures on
custom install drives.

### Running

1. Start SteamVR
2. Start MSI Afterburner and ensure hardware monitoring shared memory is available, or start HWiNFO with Shared Memory Support enabled
3. Run `vr_perf_profiler.exe`

The settings window opens on startup. The app also runs from the system tray. Use `Settings...` to choose the primary hardware source and sensor rows, and
`Connect SteamVR Overlay` for an explicit manual retry if SteamVR was not ready.

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

HUD mode defaults to `1.0 m` in front of the headset. The Web settings window
includes a HUD panel size slider from `0.50 m` to `2.50 m`. Wrist mode defaults
to a compact `0.75 m` overlay width.

Tray menu:

- `Connect SteamVR Overlay` — Initialize the OpenVR overlay on demand
- `Settings...` — Open the configuration dialog without requiring SteamVR

## Architecture

```
MSI Afterburner shared memory or HWiNFO shared memory
SteamVR compositor timing → FPS / frame interval / dropped frames
    → Exact sensor selection + VR frame metrics
    → D2D Renderer
    → OpenVR Overlay
```

- **Data Layer** — Reads the selected hardware source, keeps raw sensor rows selectable, and reads VR frame timing from SteamVR when connected
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
