#pragma once

#include <string>

namespace vrperf {

enum class SensorCategory {
    CpuLoad,
    CpuTemp,
    CpuClock,
    GpuLoad,
    GpuTemp,
    GpuClock,
    GpuMemory,
    GpuFan,
    RamUsage,
    Fan,
    Voltage,
    Power,
    Unknown
};

struct SensorReading {
    SensorCategory category = SensorCategory::Unknown;
    std::string label;
    double value = 0.0;
    std::string unit;
    int sensorId = -1;
    int readingId = -1;
};

// Map HWiNFO reading type string to our category
// HWiNFO uses strings like "Temperature", "Clock", "Load", etc.
SensorCategory ClassifySensor(const std::string& readingType,
                              const std::string& sensorName,
                              const std::string& label);

} // namespace vrperf
