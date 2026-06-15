#include "data/metric_aggregator.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace vrperf {

namespace {

std::string ToLower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });
    return text;
}

bool Contains(const std::string& text, const char* value)
{
    return text.find(value) != std::string::npos;
}

bool IsUsableReading(const SensorReading& reading)
{
    if (reading.category == SensorCategory::Unknown || !std::isfinite(reading.value)) {
        return false;
    }

    switch (reading.category) {
    case SensorCategory::CpuTemp:
    case SensorCategory::GpuTemp:
    case SensorCategory::CpuClock:
    case SensorCategory::GpuClock:
    case SensorCategory::Voltage:
    case SensorCategory::Power:
        return reading.value > 0.0;
    default:
        return true;
    }
}

int ReadingPreferenceScore(const SensorReading& reading)
{
    const auto label = ToLower(reading.label);

    switch (reading.category) {
    case SensorCategory::RamUsage:
        if (Contains(label, "virtual") || Contains(label, u8"\u865A\u62DF")) {
            return 0;
        }
        if (Contains(label, "physical") ||
            Contains(label, "total") ||
            Contains(label, u8"\u7269\u7406")) {
            return 2;
        }
        return 1;

    case SensorCategory::GpuMemory:
        if (Contains(label, "used")) {
            return 3;
        }
        if (Contains(label, "free")) {
            return 0;
        }
        if (Contains(label, "total")) {
            return 1;
        }
        return 2;

    case SensorCategory::Power:
        if (Contains(label, "package") || Contains(label, "total")) {
            return 2;
        }
        return 1;

    default:
        return 1;
    }
}

bool ShouldReplaceReading(const SensorReading& current,
                          const SensorReading& candidate)
{
    const auto currentScore = ReadingPreferenceScore(current);
    const auto candidateScore = ReadingPreferenceScore(candidate);
    return candidateScore > currentScore;
}

} // namespace

void MetricAggregator::Clear()
{
    readings_.clear();
}

void MetricAggregator::AddReadings(const std::vector<SensorReading>& readings)
{
    for (const auto& reading : readings) {
        if (!IsUsableReading(reading)) {
            continue;
        }

        const auto key = CategoryKey(reading.category);
        if (key.empty()) {
            continue;
        }

        const auto existing = std::find_if(
            readings_.begin(),
            readings_.end(),
            [&](const SensorReading& current) {
                return CategoryKey(current.category) == key;
            });

        if (existing == readings_.end()) {
            readings_.push_back(reading);
        } else if (ShouldReplaceReading(*existing, reading)) {
            *existing = reading;
        }
    }
}

std::vector<SensorReading> MetricAggregator::GetReadings() const
{
    return readings_;
}

std::string MetricAggregator::CategoryKey(SensorCategory category)
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
    case SensorCategory::Unknown: return "";
    }

    return "";
}

} // namespace vrperf
