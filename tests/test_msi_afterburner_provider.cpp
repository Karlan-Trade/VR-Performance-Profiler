#include "data/msi_afterburner_provider.h"

#include <Windows.h>
#include <cassert>
#include <cfloat>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

void SetEntry(vrperf::mahm::SharedMemoryEntry& entry,
              uint32_t sourceId,
              const char* name,
              const char* units,
              float value,
              uint32_t gpu = vrperf::mahm::kUnknownGpu)
{
    strcpy_s(entry.sourceName, name);
    strcpy_s(entry.sourceUnits, units);
    strcpy_s(entry.localizedSourceName, name);
    strcpy_s(entry.localizedSourceUnits, units);
    strcpy_s(entry.recommendedFormat, "%.1f");
    entry.data = value;
    entry.minLimit = 0.0f;
    entry.maxLimit = 100.0f;
    entry.gpu = gpu;
    entry.sourceId = sourceId;
}

} // namespace

int main()
{
    const std::wstring mappingName =
        L"Local\\VRPerfProfilerMAHMTest" + std::to_wstring(GetCurrentProcessId());
    constexpr uint32_t entryCount = 8;
    constexpr uint32_t gpuCount = 2;
    const size_t mappingSize =
        sizeof(vrperf::mahm::SharedMemoryHeader) +
        entryCount * sizeof(vrperf::mahm::SharedMemoryEntry) +
        gpuCount * sizeof(vrperf::mahm::SharedMemoryGpuEntry);

    HANDLE mapping = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        static_cast<DWORD>(mappingSize),
        mappingName.c_str());
    assert(mapping);

    auto* view = static_cast<uint8_t*>(
        MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, mappingSize));
    assert(view);

    auto* header = reinterpret_cast<vrperf::mahm::SharedMemoryHeader*>(view);
    header->signature = vrperf::mahm::kSignatureValid;
    header->version = vrperf::mahm::kMinSupportedVersion;
    header->headerSize = sizeof(vrperf::mahm::SharedMemoryHeader);
    header->numEntries = entryCount;
    header->entrySize = sizeof(vrperf::mahm::SharedMemoryEntry);
    header->numGpuEntries = gpuCount;
    header->gpuEntrySize = sizeof(vrperf::mahm::SharedMemoryGpuEntry);

    auto* entries = reinterpret_cast<vrperf::mahm::SharedMemoryEntry*>(
        view + header->headerSize);
    auto* gpus = reinterpret_cast<vrperf::mahm::SharedMemoryGpuEntry*>(
        view + header->headerSize + entryCount * header->entrySize);
    strcpy_s(gpus[0].device, "AMD Radeon 780M Graphics");
    strcpy_s(gpus[1].device, "NVIDIA GeForce RTX 4070 Laptop GPU");

    SetEntry(entries[0], vrperf::mahm::kSourceCpuTemperature,
             "CPU temperature", "C", 58.0f, 1);
    SetEntry(entries[1], vrperf::mahm::kSourceGpuUsage,
             "GPU usage", "%", 42.0f, 0);
    SetEntry(entries[2], vrperf::mahm::kSourceRamUsage,
             "RAM usage", "%", 37.5f);
    SetEntry(entries[3], vrperf::mahm::kSourceCoreClock,
             "Core clock", "MHz", 2200.0f, 0);
    SetEntry(entries[4], vrperf::mahm::kSourceGpuTemperature,
             "GPU temperature", "C", FLT_MAX, 0);
    SetEntry(entries[5], vrperf::mahm::kSourceFbUsage,
             "FB usage", "%", 12.0f, 1);
    SetEntry(entries[6], vrperf::mahm::kSourceCpuTemperature,
             "CPU1 temperature", "C", 61.0f);
    SetEntry(entries[7], vrperf::mahm::kSourceCpuUsage,
             "CPU2 usage", "%", 17.0f);

    vrperf::MsiAfterburnerProvider provider(mappingName);
    assert(provider.Refresh());
    assert(provider.IsAvailable());

    const auto readings = provider.GetReadings();
    assert(readings.size() == 5);

    bool sawCpuTemp = false;
    bool sawGpuLoad = false;
    bool sawRam = false;
    bool sawGpuClock = false;
    bool sawFbUsage = false;

    for (const auto& reading : readings) {
        if (reading.category == vrperf::SensorCategory::CpuTemp) {
            sawCpuTemp = true;
            assert(reading.value == 58.0);
            assert(reading.unit == "C");
            assert(reading.device.empty());
            assert(reading.label == "CPU temperature");
        } else if (reading.category == vrperf::SensorCategory::GpuLoad) {
            sawGpuLoad = true;
            assert(reading.value == 42.0);
            assert(reading.sensorId == 0);
            assert(reading.device == "GPU1: AMD Radeon 780M Graphics");
        } else if (reading.category == vrperf::SensorCategory::RamUsage) {
            sawRam = true;
            assert(reading.value == 37.5);
        } else if (reading.category == vrperf::SensorCategory::GpuClock) {
            sawGpuClock = true;
            assert(reading.value == 2200.0);
        } else if (reading.label == "FB usage") {
            sawFbUsage = true;
            assert(reading.category == vrperf::SensorCategory::Unknown);
            assert(reading.sensorId == 1);
            assert(reading.device == "GPU2: NVIDIA GeForce RTX 4070 Laptop GPU");
        }

        assert(reading.label != "CPU1 temperature");
        assert(reading.label != "CPU2 usage");
    }

    assert(sawCpuTemp);
    assert(sawGpuLoad);
    assert(sawRam);
    assert(sawGpuClock);
    assert(sawFbUsage);

    provider.Close();
    UnmapViewOfFile(view);
    CloseHandle(mapping);

    std::cout << "[PASS] MSI Afterburner provider tests passed\n";
    return 0;
}
