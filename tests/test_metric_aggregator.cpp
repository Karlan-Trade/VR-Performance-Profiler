#include "data/metric_aggregator.h"

#include <cassert>
#include <iostream>

namespace {

vrperf::SensorReading MakeReading(vrperf::SensorCategory category,
                                  const char* label,
                                  double value,
                                  const char* unit)
{
    vrperf::SensorReading reading;
    reading.category = category;
    reading.label = label;
    reading.value = value;
    reading.unit = unit;
    return reading;
}

} // namespace

int main()
{
    vrperf::MetricAggregator aggregator;

    aggregator.AddReadings({
        MakeReading(vrperf::SensorCategory::CpuTemp, "LHM CPU", 61.0, "C"),
        MakeReading(vrperf::SensorCategory::GpuTemp, "LHM GPU", 58.0, "C"),
    });

    aggregator.AddReadings({
        MakeReading(vrperf::SensorCategory::CpuTemp, "HWiNFO CPU", 67.0, "C"),
        MakeReading(vrperf::SensorCategory::RamUsage, "RAM", 43.0, "%"),
    });

    const auto readings = aggregator.GetReadings();
    assert(readings.size() == 3);

    bool sawCpuTemp = false;
    bool sawGpuTemp = false;
    bool sawRam = false;

    for (const auto& reading : readings) {
        if (reading.category == vrperf::SensorCategory::CpuTemp) {
            sawCpuTemp = true;
            assert(reading.label == "LHM CPU");
            assert(reading.value == 61.0);
        } else if (reading.category == vrperf::SensorCategory::GpuTemp) {
            sawGpuTemp = true;
        } else if (reading.category == vrperf::SensorCategory::RamUsage) {
            sawRam = true;
        }
    }

    assert(sawCpuTemp);
    assert(sawGpuTemp);
    assert(sawRam);

    std::cout << "[PASS] MetricAggregator tests passed\n";
    return 0;
}
