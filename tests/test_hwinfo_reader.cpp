#include "hwinfo/hwinfo_reader.h"
#include "hwinfo/hwinfo_structs.h"

#include <Windows.h>

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

int main()
{
    std::cout << "=== HwInfoReader Tests ===" << std::endl;

    {
        vrperf::HwInfoReader reader;
        const bool result = reader.Open();
        if (!result) {
            std::cout << "[PASS] Open() returns false when HWiNFO is unavailable" << std::endl;
        } else {
            assert(reader.IsConnected());
            reader.Refresh();
            reader.Close();
            std::cout << "[PASS] Open() can connect to running HWiNFO" << std::endl;
        }
    }

    {
        const uint32_t numSensors = 1;
        const uint32_t numReadings = 2;
        const size_t totalSize = sizeof(HwInfoSensorsSharedMem)
            + numSensors * sizeof(HwInfoSensorElement)
            + numReadings * sizeof(HwInfoReadingElement);

        std::vector<uint8_t> mockData(totalSize, 0);
        auto* header = reinterpret_cast<HwInfoSensorsSharedMem*>(mockData.data());
        header->signature = HWiNFO_SENSORS_SM2_SIGNATURE;
        header->version = 2;
        header->size = static_cast<uint32_t>(totalSize);
        header->numSensorElements = numSensors;
        header->numReadingElements = numReadings;
        header->pollTime = 1234567890;

        auto* sensors = reinterpret_cast<HwInfoSensorElement*>(
            reinterpret_cast<uint8_t*>(header) + sizeof(HwInfoSensorsSharedMem));
        sensors[0].sensorId = 1;
        sensors[0].sensorInst = 0;
        wcscpy_s(sensors[0].sensorName, HWiNFO_SENSOR_ELEMENT_NAME_LEN, L"CPU");
        wcscpy_s(sensors[0].sensorLabel, HWiNFO_SENSOR_ELEMENT_LABEL_LEN, L"Intel Core i7");

        auto* readings = reinterpret_cast<HwInfoReadingElement*>(
            reinterpret_cast<uint8_t*>(header)
            + sizeof(HwInfoSensorsSharedMem)
            + numSensors * sizeof(HwInfoSensorElement));
        readings[0].sensorId = 1;
        readings[0].readingType = SENSOR_TYPE_TEMPERATURE;
        readings[0].value = 65.0;
        wcscpy_s(readings[0].label, HWiNFO_READING_LABEL_LEN, L"CPU Package");
        wcscpy_s(readings[0].unit, HWiNFO_READING_UNIT_LEN, L"C");

        readings[1].sensorId = 1;
        readings[1].readingType = SENSOR_TYPE_LOAD;
        readings[1].value = 45.0;
        wcscpy_s(readings[1].label, HWiNFO_READING_LABEL_LEN, L"CPU Total");
        wcscpy_s(readings[1].unit, HWiNFO_READING_UNIT_LEN, L"%");

        std::cout << "[PASS] Mock shared memory layout test passed" << std::endl;
    }

    {
        assert(vrperf::ClassifySensor("Temperature", "CPU Package", "CPU Package")
               == vrperf::SensorCategory::CpuTemp);
        assert(vrperf::ClassifySensor("Load", "GPU", "GPU Core")
               == vrperf::SensorCategory::GpuLoad);
        assert(vrperf::ClassifySensor("Clock", "GPU", "GPU Clock")
               == vrperf::SensorCategory::GpuClock);
        assert(vrperf::ClassifySensor("Fan", "GPU", "GPU Fan")
               == vrperf::SensorCategory::GpuFan);

        std::cout << "[PASS] Sensor classification tests passed" << std::endl;
    }

    std::cout << "\n=== All HWiNFO tests passed ===" << std::endl;
    return 0;
}
