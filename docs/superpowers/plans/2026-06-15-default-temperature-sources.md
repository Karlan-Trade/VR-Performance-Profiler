# Default Temperature Sources Implementation Plan

**Goal:** Make temperature monitoring part of the default metric set while keeping HWiNFO optional.

**Decision:** The C++ OpenVR overlay process stays native and lightweight. Temperature data is supplied through a provider chain:

```text
LibreHardwareMonitor bridge -> HWiNFO optional enrichment -> Windows fallback
```

LibreHardwareMonitor is used through a bridge snapshot file instead of being embedded directly into the C++ process. This keeps .NET/runtime/admin-permission concerns outside the SteamVR overlay loop. The helper is implemented in C# with `LibreHardwareMonitorLib` and writes the bridge JSON file consumed by the native app.

## Provider Contract

- `WindowsFallbackProvider`
  - Always attempted.
  - Supplies CPU load and RAM usage.
  - Does not supply temperature.
- `LibreHardwareMonitorBridgeProvider`
  - Default temperature bridge.
  - Reads `%LOCALAPPDATA%\VRPerfProfiler\lhm-sensors.json`.
  - Accepts either a top-level JSON array or an object with a `readings` array.
  - Supported categories: `cpu_temp`, `gpu_temp`, `cpu_load`, `gpu_load`, `gpu_memory`, `gpu_fan`, `ram_usage`, `power`, `voltage`.
  - Ignores stale files to avoid showing old thermal data.
- `HwInfoReader`
  - Optional enrichment source.
  - Used when HWiNFO is running and shared memory is enabled.
  - No longer required for default app startup.

## Implementation Tasks

- [x] Add `MetricAggregator` to merge provider readings by metric category.
- [x] Add `LibreHardwareMonitorBridgeProvider` for JSON bridge snapshots.
- [x] Update `App::UpdateOverlay()` to merge providers instead of choosing HWiNFO or Windows fallback exclusively.
- [x] Keep CPU/GPU temperature enabled in default config.
- [x] Add tests for aggregator precedence and LibreHardwareMonitor bridge parsing.
- [x] Add a real C# LibreHardwareMonitor helper once a .NET SDK is available in the environment.
- [ ] Add NVIDIA NVML and AMD ADLX/ADL providers for lower-latency vendor GPU telemetry.
- [x] Verify native app startup with a real bridge helper snapshot outside SteamVR.
- [ ] Manually verify overlay rendering inside SteamVR.

## Bridge JSON Example

```json
{
  "readings": [
    { "category": "cpu_temp", "label": "CPU Package", "value": 68.0, "unit": "C" },
    { "category": "gpu_temp", "label": "GPU Core", "value": 61.0, "unit": "C" },
    { "category": "gpu_load", "label": "GPU Load", "value": 74.0, "unit": "%" }
  ]
}
```

## Acceptance

- App starts without HWiNFO.
- CPU load and RAM still appear from Windows fallback.
- If the LibreHardwareMonitor bridge JSON exists and is fresh, CPU/GPU temperature are displayed through the default metric configuration.
- If HWiNFO is running, it can fill categories not supplied by the bridge.
- Tests pass through CTest.
