#pragma once

#include "hwinfo/sensor_data.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace vrperf {

class LibreHardwareMonitorBridgeProvider {
public:
    LibreHardwareMonitorBridgeProvider();
    explicit LibreHardwareMonitorBridgeProvider(
        std::filesystem::path snapshotPath,
        std::chrono::milliseconds maxAge = std::chrono::milliseconds(5000));

    bool Refresh();
    bool IsAvailable() const;
    std::vector<SensorReading> GetReadings() const;

    static std::filesystem::path GetDefaultSnapshotPath();
    static SensorCategory CategoryFromString(const std::string& category);

private:
    std::filesystem::path snapshotPath_;
    std::chrono::milliseconds maxAge_;
    bool available_ = false;
    std::vector<SensorReading> readings_;
};

} // namespace vrperf
