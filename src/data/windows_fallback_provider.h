#pragma once

#include "hwinfo/sensor_data.h"

#include <Windows.h>
#include <vector>

namespace vrperf {

class WindowsFallbackProvider {
public:
    bool Refresh();
    std::vector<SensorReading> GetReadings() const;

private:
    bool hasPreviousCpuTimes_ = false;
    FILETIME previousIdleTime_{};
    FILETIME previousKernelTime_{};
    FILETIME previousUserTime_{};
    std::vector<SensorReading> readings_;

    static unsigned long long ToUInt64(const FILETIME& ft);
};

} // namespace vrperf
