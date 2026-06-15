#pragma once

#include "hwinfo/hwinfo_structs.h"
#include "hwinfo/sensor_data.h"

#include <Windows.h>
#include <vector>
#include <string>
#include <mutex>

namespace vrperf {

class HwInfoReader {
public:
    HwInfoReader();
    ~HwInfoReader();

    // Open shared memory mapping. Returns false if HWiNFO is not running.
    bool Open();

    // Close the mapping.
    void Close();

    // Check if connected to HWiNFO shared memory.
    bool IsConnected() const;

    // Re-read the shared memory. Returns true if data was refreshed.
    bool Refresh();

    // Get the latest sensor readings.
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
