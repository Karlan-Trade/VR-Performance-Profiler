#pragma once

#include "data/sensor_data.h"
#include "hwinfo/hwinfo_structs.h"

#include <Windows.h>

#include <mutex>
#include <string>
#include <vector>

namespace vrperf {

SensorCategory ClassifySensor(const std::string& readingType,
                              const std::string& sensorName,
                              const std::string& label);

class HwInfoReader {
public:
    HwInfoReader();
    ~HwInfoReader();

    bool Open();
    void Close();
    bool IsConnected() const;
    bool Refresh();
    std::vector<SensorReading> GetReadings() const;

private:
    bool MapSharedMemory();
    void ParseSharedMemory();

    HANDLE hMapping_ = nullptr;
    const HwInfoSensorsSharedMem* pSharedMem_ = nullptr;
    std::vector<SensorReading> readings_;
    mutable std::mutex readingsMutex_;
    uint64_t lastPollTime_ = 0;
};

} // namespace vrperf
