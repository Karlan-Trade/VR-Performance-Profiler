#include "data/libre_hardware_monitor_bridge_provider.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

int main()
{
    const auto path = std::filesystem::temp_directory_path()
        / "vrperf_lhm_bridge_test.json";

    {
        std::ofstream file(path);
        file << R"({
            "readings": [
                { "category": "cpu_temp", "device": "CPU", "label": "CPU Package", "value": 68.0, "unit": "C" },
                { "category": "gpu_temp", "device": "GPU2", "label": "GPU2 GPU Core", "value": 62.0, "unit": "C" },
                { "category": "unknown", "label": "Ignored", "value": 1.0, "unit": "" }
            ]
        })";
    }

    vrperf::LibreHardwareMonitorBridgeProvider provider(
        path,
        std::chrono::hours(1));

    assert(provider.Refresh());
    assert(provider.IsAvailable());

    const auto readings = provider.GetReadings();
    assert(readings.size() == 2);

    bool sawCpuTemp = false;
    bool sawGpuTemp = false;
    for (const auto& reading : readings) {
        if (reading.category == vrperf::SensorCategory::CpuTemp) {
            sawCpuTemp = true;
            assert(reading.device == "CPU");
            assert(reading.label == "CPU Package");
            assert(reading.value == 68.0);
        } else if (reading.category == vrperf::SensorCategory::GpuTemp) {
            sawGpuTemp = true;
            assert(reading.device == "GPU2");
            assert(reading.label == "GPU2 GPU Core");
            assert(reading.value == 62.0);
        }
    }

    assert(sawCpuTemp);
    assert(sawGpuTemp);

    std::filesystem::remove(path);

    std::cout << "[PASS] LibreHardwareMonitor bridge provider tests passed\n";
    return 0;
}
