#include "hwinfo/hwinfo_reader.h"

#include <algorithm>
#include <cwctype>

namespace vrperf {

// ──────────────────────────────────────────────────────────────
// Sensor classification helper
// ──────────────────────────────────────────────────────────────

static std::wstring ToLower(const std::wstring& s)
{
    std::wstring result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](wchar_t c) { return std::towlower(c); });
    return result;
}

SensorCategory ClassifySensor(const std::string& readingType,
                              const std::string& sensorName,
                              const std::string& label)
{
    // readingType matches HwInfoReadingType enum values
    // We also check sensorName and label for context

    // Temperature
    if (readingType == "Temperature" || readingType == "1") {
        if (sensorName.find("GPU") != std::string::npos)
            return SensorCategory::GpuTemp;
        if (sensorName.find("CPU") != std::string::npos ||
            sensorName.find("Core") != std::string::npos ||
            sensorName.find("Package") != std::string::npos)
            return SensorCategory::CpuTemp;
        return SensorCategory::CpuTemp; // Default temp to CPU
    }

    // Load
    if (readingType == "Load" || readingType == "7") {
        if (sensorName.find("GPU") != std::string::npos)
            return SensorCategory::GpuLoad;
        if (sensorName.find("CPU") != std::string::npos ||
            sensorName.find("Core") != std::string::npos)
            return SensorCategory::CpuLoad;
        if (sensorName.find("Memory") != std::string::npos)
            return SensorCategory::RamUsage;
        return SensorCategory::CpuLoad;
    }

    // Clock
    if (readingType == "Clock" || readingType == "6") {
        if (sensorName.find("GPU") != std::string::npos)
            return SensorCategory::GpuClock;
        return SensorCategory::CpuClock;
    }

    // Fan
    if (readingType == "Fan" || readingType == "3") {
        if (sensorName.find("GPU") != std::string::npos)
            return SensorCategory::GpuFan;
        return SensorCategory::Fan;
    }

    // Power
    if (readingType == "Power" || readingType == "5")
        return SensorCategory::Power;

    // Voltage
    if (readingType == "Voltage" || readingType == "2")
        return SensorCategory::Voltage;

    // Data (memory usage in MB/GB)
    if (readingType == "Data" || readingType == "13") {
        if (sensorName.find("GPU") != std::string::npos)
            return SensorCategory::GpuMemory;
        return SensorCategory::RamUsage;
    }

    return SensorCategory::Unknown;
}

// Helper: convert HwInfoReadingType enum to string for classification
static std::string ReadingTypeToString(HwInfoReadingType type)
{
    switch (type) {
    case SENSOR_TYPE_TEMPERATURE: return "Temperature";
    case SENSOR_TYPE_VOLTAGE:     return "Voltage";
    case SENSOR_TYPE_FAN:         return "Fan";
    case SENSOR_TYPE_CURRENT:     return "Current";
    case SENSOR_TYPE_POWER:       return "Power";
    case SENSOR_TYPE_CLOCK:       return "Clock";
    case SENSOR_TYPE_LOAD:        return "Load";
    case SENSOR_TYPE_DATA:        return "Data";
    case SENSOR_TYPE_SMALL_DATA:  return "SmallData";
    case SENSOR_TYPE_THROUGHPUT:  return "Throughput";
    case SENSOR_TYPE_CONTROL:     return "Control";
    case SENSOR_TYPE_LEVEL:       return "Level";
    case SENSOR_TYPE_FACTOR:      return "Factor";
    default:                      return "Unknown";
    }
}

// Helper: wide string to narrow
static std::string WideToNarrow(const wchar_t* ws)
{
    if (!ws) return "";
    std::string result;
    while (*ws) {
        result += static_cast<char>(*ws & 0x7F);
        ++ws;
    }
    return result;
}

// ──────────────────────────────────────────────────────────────
// HwInfoReader implementation
// ──────────────────────────────────────────────────────────────

HwInfoReader::HwInfoReader() = default;

HwInfoReader::~HwInfoReader()
{
    Close();
}

bool HwInfoReader::Open()
{
    if (hMapping_) {
        return true; // Already open
    }

    hMapping_ = OpenFileMappingW(FILE_MAP_READ, FALSE, L"HWiNFO_SENS_SM2");
    if (!hMapping_) {
        return false; // HWiNFO not running or shared memory not available
    }

    if (!MapSharedMemory()) {
        Close();
        return false;
    }

    return true;
}

void HwInfoReader::Close()
{
    if (pSharedMem_) {
        UnmapViewOfFile(pSharedMem_);
        pSharedMem_ = nullptr;
    }
    if (hMapping_) {
        CloseHandle(hMapping_);
        hMapping_ = nullptr;
    }
    lastPollTime_ = 0;
}

bool HwInfoReader::IsConnected() const
{
    return hMapping_ != nullptr && pSharedMem_ != nullptr;
}

bool HwInfoReader::MapSharedMemory()
{
    pSharedMem_ = static_cast<const HwInfoSensorsSharedMem*>(
        MapViewOfFile(hMapping_, FILE_MAP_READ, 0, 0, 0)
    );

    if (!pSharedMem_) {
        return false;
    }

    // Validate signature
    if (pSharedMem_->signature != HWiNFO_SENSORS_SM2_SIGNATURE) {
        UnmapViewOfFile(pSharedMem_);
        pSharedMem_ = nullptr;
        return false;
    }

    return true;
}

bool HwInfoReader::Refresh()
{
    if (!IsConnected()) {
        // Try to reconnect
        if (!Open()) {
            return false;
        }
    }

    // Check if data has changed
    if (pSharedMem_->pollTime == lastPollTime_) {
        return false; // No new data
    }

    lastPollTime_ = pSharedMem_->pollTime;
    ParseSharedMemory();
    return true;
}

void HwInfoReader::ParseSharedMemory()
{
    if (!pSharedMem_) return;

    const auto* sensors = GetSensorElements(pSharedMem_);
    const auto* readings = GetReadingElements(pSharedMem_);

    std::vector<SensorReading> newReadings;
    newReadings.reserve(pSharedMem_->numReadingElements);

    for (uint32_t i = 0; i < pSharedMem_->numReadingElements; ++i) {
        const auto& re = readings[i];

        // Skip readings with no value
        if (re.value == 0.0 && re.readingType == SENSOR_TYPE_NONE) {
            continue;
        }

        SensorReading sr;
        sr.readingId = static_cast<int>(i);
        sr.sensorId = static_cast<int>(re.sensorId);
        sr.source = "HWiNFO";
        sr.value = re.value;
        sr.unit = WideToNarrow(re.unit);
        sr.label = WideToNarrow(re.label);

        // Find the parent sensor name for classification
        std::string sensorName;
        for (uint32_t j = 0; j < pSharedMem_->numSensorElements; ++j) {
            if (sensors[j].sensorId == re.sensorId) {
                sensorName = WideToNarrow(sensors[j].sensorName);
                break;
            }
        }

        sr.category = ClassifySensor(
            ReadingTypeToString(re.readingType),
            sensorName,
            sr.label
        );

        newReadings.push_back(std::move(sr));
    }

    // Swap readings under lock
    {
        std::lock_guard<std::mutex> lock(readingsMutex_);
        readings_ = std::move(newReadings);
    }
}

std::vector<SensorReading> HwInfoReader::GetReadings() const
{
    std::lock_guard<std::mutex> lock(readingsMutex_);
    return readings_;
}

} // namespace vrperf
