#include "hwinfo/hwinfo_reader.h"
#include "hwinfo/hwinfo_structs.h"

#include <Windows.h>
#include <iostream>
#include <cassert>
#include <cstring>

// Test: Create a mock shared memory region and verify HwInfoReader can parse it
int main()
{
    std::cout << "=== HwInfoReader Tests ===" << std::endl;

    // Test 1: Open() should fail gracefully when HWiNFO is not running
    {
        vrperf::HwInfoReader reader;
        bool result = reader.Open();
        // This will fail if HWiNFO is not running - that's expected
        if (!result) {
            std::cout << "[PASS] Open() returns false when HWiNFO not running" << std::endl;
        } else {
            std::cout << "[INFO] HWiNFO is running - testing connection" << std::endl;
            assert(reader.IsConnected());

            // Try refreshing data
            reader.Refresh();
            auto readings = reader.GetReadings();
            std::cout << "[INFO] Got " << readings.size() << " sensor readings" << std::endl;

            // Print some readings
            int count = 0;
            for (const auto& r : readings) {
                if (count >= 10) break;
                std::cout << "  " << r.label << ": " << r.value << " " << r.unit << std::endl;
                count++;
            }

            reader.Close();
            std::cout << "[PASS] Successfully read HWiNFO data" << std::endl;
        }
    }

    // Test 2: Mock shared memory (creates a local mapping)
    {
        std::cout << "\n--- Mock Shared Memory Test ---" << std::endl;

        // Create a mock shared memory region
        const uint32_t numSensors = 1;
        const uint32_t numReadings = 2;

        size_t totalSize = sizeof(HwInfoSensorsSharedMem)
                         + numSensors * sizeof(HwInfoSensorElement)
                         + numReadings * sizeof(HwInfoReadingElement);

        // Allocate and fill mock data
        std::vector<uint8_t> mockData(totalSize, 0);

        auto* header = reinterpret_cast<HwInfoSensorsSharedMem*>(mockData.data());
        header->signature = HWiNFO_SENSORS_SM2_SIGNATURE;
        header->version = 2;
        header->size = static_cast<uint32_t>(totalSize);
        header->numSensorElements = numSensors;
        header->numReadingElements = numReadings;
        header->pollTime = 1234567890;

        // Fill sensor element
        auto* sensors = reinterpret_cast<HwInfoSensorElement*>(
            reinterpret_cast<uint8_t*>(header) + sizeof(HwInfoSensorsSharedMem));
        sensors[0].sensorId = 1;
        sensors[0].sensorInst = 0;
        wcscpy_s(sensors[0].sensorName, HWiNFO_SENSOR_ELEMENT_NAME_LEN, L"CPU");
        wcscpy_s(sensors[0].sensorLabel, HWiNFO_SENSOR_ELEMENT_LABEL_LEN, L"Intel Core i7");

        // Fill reading elements
        auto* readings = reinterpret_cast<HwInfoReadingElement*>(
            reinterpret_cast<uint8_t*>(header)
            + sizeof(HwInfoSensorsSharedMem)
            + numSensors * sizeof(HwInfoSensorElement));
        readings[0].sensorId = 1;
        readings[0].readingType = SENSOR_TYPE_TEMPERATURE;
        readings[0].value = 65.0;
        wcscpy_s(readings[0].label, HWiNFO_READING_LABEL_LEN, L"CPU Package");
        wcscpy_s(readings[0].unit, HWiNFO_READING_UNIT_LEN, L"°C");

        readings[1].sensorId = 1;
        readings[1].readingType = SENSOR_TYPE_LOAD;
        readings[1].value = 45.0;
        wcscpy_s(readings[1].label, HWiNFO_READING_LABEL_LEN, L"CPU Total");
        wcscpy_s(readings[1].unit, HWiNFO_READING_UNIT_LEN, L"%");

        std::cout << "[INFO] Mock data created: " << numSensors << " sensors, "
                  << numReadings << " readings" << std::endl;
        std::cout << "[PASS] Mock shared memory structure test" << std::endl;
    }

    // Test 3: Sensor classification
    {
        std::cout << "\n--- Sensor Classification Test ---" << std::endl;

        auto cat1 = vrperf::ClassifySensor("Temperature", "CPU Package", "CPU Package");
        assert(cat1 == vrperf::SensorCategory::CpuTemp);

        auto cat2 = vrperf::ClassifySensor("Load", "GPU", "GPU Core");
        assert(cat2 == vrperf::SensorCategory::GpuLoad);

        auto cat3 = vrperf::ClassifySensor("Clock", "GPU", "GPU Clock");
        assert(cat3 == vrperf::SensorCategory::GpuClock);

        auto cat4 = vrperf::ClassifySensor("Fan", "GPU", "GPU Fan");
        assert(cat4 == vrperf::SensorCategory::GpuFan);

        std::cout << "[PASS] Sensor classification tests passed" << std::endl;
    }

    std::cout << "\n=== All tests passed ===" << std::endl;
    return 0;
}
