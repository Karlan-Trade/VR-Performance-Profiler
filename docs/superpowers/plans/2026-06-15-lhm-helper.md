# LibreHardwareMonitor Helper Implementation Plan

**Goal:** Turn the existing LibreHardwareMonitor bridge protocol into a working helper that writes real sensor snapshots for the native SteamVR overlay process.

## Scope

- Add a .NET 8 console helper under `tools/lhm_bridge`.
- Reference `LibreHardwareMonitorLib`.
- Emit `%LOCALAPPDATA%\VRPerfProfiler\lhm-sensors.json`.
- Keep the helper separate from the native OpenVR process.
- Let CMake build and copy the helper when `dotnet` is available.
- Let the native app auto-start the helper when it is present next to the exe.

## Acceptance

- [x] `dotnet build tools/lhm_bridge/VRPerfProfiler.LhmBridge.csproj` succeeds.
- [x] `VRPerfProfiler.LhmBridge --once` writes bridge JSON.
- [x] CMake publishes the helper to `lhm_bridge` next to the native exe.
- [x] Native app auto-starts the helper when it exists.
- [x] Native C++ build and tests still pass.
- [x] Running the native app without SteamVR still starts normally and can consume the helper snapshot.
