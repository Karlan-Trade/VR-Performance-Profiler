#include "data/metric_aggregator.h"

#include <algorithm>

namespace vrperf {

void MetricAggregator::Clear()
{
    readings_.clear();
}

void MetricAggregator::AddReadings(const std::vector<SensorReading>& readings)
{
    for (const auto& reading : readings) {
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
    case SensorCategory::Unknown: return "";
    }

    return "";
}

} // namespace vrperf
