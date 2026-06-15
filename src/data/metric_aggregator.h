#pragma once

#include "hwinfo/sensor_data.h"

#include <string>
#include <vector>

namespace vrperf {

class MetricAggregator {
public:
    void Clear();
    void AddReadings(const std::vector<SensorReading>& readings);
    std::vector<SensorReading> GetReadings() const;

    static std::string CategoryKey(SensorCategory category);

private:
    std::vector<SensorReading> readings_;
};

} // namespace vrperf
