#pragma once

#include "data/sensor_data.h"

#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>

namespace vrperf {

namespace mahm {

static constexpr uint32_t kSignatureValid = 0x4D41484D; // SDK 'MAHM'
static constexpr uint32_t kSignatureDead = 0x0000DEAD;
static constexpr uint32_t kMinSupportedVersion = 0x00020000;
static constexpr uint32_t kUnknownGpu = 0xFFFFFFFF;

static constexpr uint32_t kSourceGpuTemperature = 0x00000000;
static constexpr uint32_t kSourcePcbTemperature = 0x00000001;
static constexpr uint32_t kSourceMemTemperature = 0x00000002;
static constexpr uint32_t kSourceVrmTemperature = 0x00000003;
static constexpr uint32_t kSourceFanSpeed = 0x00000010;
static constexpr uint32_t kSourceFanTachometer = 0x00000011;
static constexpr uint32_t kSourceFanSpeed2 = 0x00000012;
static constexpr uint32_t kSourceFanTachometer2 = 0x00000013;
static constexpr uint32_t kSourceFanSpeed3 = 0x00000014;
static constexpr uint32_t kSourceFanTachometer3 = 0x00000015;
static constexpr uint32_t kSourceCoreClock = 0x00000020;
static constexpr uint32_t kSourceShaderClock = 0x00000021;
static constexpr uint32_t kSourceMemoryClock = 0x00000022;
static constexpr uint32_t kSourceGpuUsage = 0x00000030;
static constexpr uint32_t kSourceMemoryUsage = 0x00000031;
static constexpr uint32_t kSourceFbUsage = 0x00000032;
static constexpr uint32_t kSourceVidUsage = 0x00000033;
static constexpr uint32_t kSourceBusUsage = 0x00000034;
static constexpr uint32_t kSourceMemoryUsageProcess = 0x00000035;
static constexpr uint32_t kSourceGpuVoltage = 0x00000040;
static constexpr uint32_t kSourceGpuRelPower = 0x00000060;
static constexpr uint32_t kSourceGpuAbsPower = 0x00000061;
static constexpr uint32_t kSourceCpuTemperature = 0x00000080;
static constexpr uint32_t kSourceCpuUsage = 0x00000090;
static constexpr uint32_t kSourceRamUsage = 0x00000091;
static constexpr uint32_t kSourceCpuClock = 0x000000A0;
static constexpr uint32_t kSourceGpuTemperature2 = 0x000000B0;
static constexpr uint32_t kSourcePcbTemperature2 = 0x000000B1;
static constexpr uint32_t kSourceMemTemperature2 = 0x000000B2;
static constexpr uint32_t kSourceVrmTemperature2 = 0x000000B3;
static constexpr uint32_t kSourceGpuTemperature3 = 0x000000C0;
static constexpr uint32_t kSourcePcbTemperature3 = 0x000000C1;
static constexpr uint32_t kSourceMemTemperature3 = 0x000000C2;
static constexpr uint32_t kSourceVrmTemperature3 = 0x000000C3;
static constexpr uint32_t kSourceGpuTemperature4 = 0x000000D0;
static constexpr uint32_t kSourcePcbTemperature4 = 0x000000D1;
static constexpr uint32_t kSourceMemTemperature4 = 0x000000D2;
static constexpr uint32_t kSourceVrmTemperature4 = 0x000000D3;
static constexpr uint32_t kSourceGpuTemperature5 = 0x000000E0;
static constexpr uint32_t kSourcePcbTemperature5 = 0x000000E1;
static constexpr uint32_t kSourceMemTemperature5 = 0x000000E2;
static constexpr uint32_t kSourceVrmTemperature5 = 0x000000E3;
static constexpr uint32_t kSourceCpuPower = 0x00000100;

struct SharedMemoryHeader {
    uint32_t signature;
    uint32_t version;
    uint32_t headerSize;
    uint32_t numEntries;
    uint32_t entrySize;
    int32_t time;
    uint32_t numGpuEntries;
    uint32_t gpuEntrySize;
};

struct SharedMemoryEntry {
    char sourceName[MAX_PATH];
    char sourceUnits[MAX_PATH];
    char localizedSourceName[MAX_PATH];
    char localizedSourceUnits[MAX_PATH];
    char recommendedFormat[MAX_PATH];
    float data;
    float minLimit;
    float maxLimit;
    uint32_t flags;
    uint32_t gpu;
    uint32_t sourceId;
};

struct SharedMemoryGpuEntry {
    char gpuId[MAX_PATH];
    char family[MAX_PATH];
    char device[MAX_PATH];
    char driver[MAX_PATH];
    char bios[MAX_PATH];
    uint32_t memoryAmountKb;
};

static_assert(sizeof(SharedMemoryHeader) == 32);
static_assert(sizeof(SharedMemoryEntry) == 1324);
static_assert(sizeof(SharedMemoryGpuEntry) == 1304);

} // namespace mahm

class MsiAfterburnerProvider {
public:
    MsiAfterburnerProvider();
    explicit MsiAfterburnerProvider(std::wstring mappingName);
    ~MsiAfterburnerProvider();

    MsiAfterburnerProvider(const MsiAfterburnerProvider&) = delete;
    MsiAfterburnerProvider& operator=(const MsiAfterburnerProvider&) = delete;

    bool Refresh();
    bool IsAvailable() const;
    std::vector<SensorReading> GetReadings() const;
    void Close();

    static SensorCategory CategoryFromSourceId(uint32_t sourceId);

private:
    bool Open();
    bool IsHeaderUsable(const mahm::SharedMemoryHeader& header) const;
    static bool IsValueUsable(SensorCategory category, double value);
    static std::string StringFromBuffer(const char* text, size_t capacity);
    static std::string LabelForEntry(const mahm::SharedMemoryEntry& entry);
    static std::string UnitForEntry(const mahm::SharedMemoryEntry& entry);
    std::vector<std::string> ReadGpuNames(const mahm::SharedMemoryHeader& header) const;

    std::wstring mappingName_;
    HANDLE mapping_ = nullptr;
    const uint8_t* view_ = nullptr;
    bool available_ = false;
    std::vector<SensorReading> readings_;
};

} // namespace vrperf
