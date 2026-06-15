#pragma once

#include <sstream>
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
    VrFps,
    VrFrameTime,
    VrGpuFrameTime,
    VrRefreshRate,
    VrDroppedFrames,
    Unknown
};

struct SensorReading {
    SensorCategory category = SensorCategory::Unknown;
    std::string source;
    std::string device;
    std::string label;
    double value = 0.0;
    std::string unit;
    int sensorId = -1;
    int readingId = -1;
};

inline std::string SensorCategoryKey(SensorCategory category)
{
    switch (category) {
    case SensorCategory::CpuLoad: return "cpu_load";
    case SensorCategory::CpuTemp: return "cpu_temp";
    case SensorCategory::CpuClock: return "cpu_clock";
    case SensorCategory::GpuLoad: return "gpu_load";
    case SensorCategory::GpuTemp: return "gpu_temp";
    case SensorCategory::GpuClock: return "gpu_clock";
    case SensorCategory::GpuMemory: return "gpu_memory";
    case SensorCategory::GpuFan: return "gpu_fan";
    case SensorCategory::RamUsage: return "ram_usage";
    case SensorCategory::Fan: return "fan";
    case SensorCategory::Voltage: return "voltage";
    case SensorCategory::Power: return "power";
    case SensorCategory::VrFps: return "vr_fps";
    case SensorCategory::VrFrameTime: return "vr_frame_time";
    case SensorCategory::VrGpuFrameTime: return "vr_gpu_frame_time";
    case SensorCategory::VrRefreshRate: return "vr_refresh_rate";
    case SensorCategory::VrDroppedFrames: return "vr_dropped_frames";
    case SensorCategory::Unknown: return "unknown";
    }

    return "unknown";
}

inline std::string HexEncodeSensorKeyPart(const std::string& text)
{
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(text.size() * 2);
    for (unsigned char ch : text) {
        encoded.push_back(kHex[(ch >> 4) & 0x0F]);
        encoded.push_back(kHex[ch & 0x0F]);
    }
    return encoded;
}

inline std::string LegacySensorReadingKey(const SensorReading& reading)
{
    std::ostringstream key;
    key << reading.source << '|'
        << SensorCategoryKey(reading.category) << '|'
        << reading.sensorId << '|'
        << reading.readingId << '|'
        << reading.label;
    return key.str();
}

inline std::string SensorReadingKey(const SensorReading& reading)
{
    std::ostringstream key;
    key << "v2|"
        << HexEncodeSensorKeyPart(reading.source) << '|'
        << SensorCategoryKey(reading.category) << '|'
        << reading.sensorId << '|'
        << reading.readingId << '|'
        << HexEncodeSensorKeyPart(reading.label);
    return key.str();
}

// Map HWiNFO reading type string to our category
// HWiNFO uses strings like "Temperature", "Clock", "Load", etc.
SensorCategory ClassifySensor(const std::string& readingType,
                              const std::string& sensorName,
                              const std::string& label);

} // namespace vrperf
