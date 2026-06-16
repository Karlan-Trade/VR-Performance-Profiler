#pragma once

#include "data/sensor_data.h"

#include <string>
#include <vector>

namespace vrperf {

struct PerfSnapshot {
    std::vector<SensorReading> readings;
    double vrFrameTimeMs = 0.0;
    double vrDroppedFrames = 0.0;
    std::string statusText;
};

} // namespace vrperf
