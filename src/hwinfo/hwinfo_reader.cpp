#include "hwinfo/hwinfo_reader.h"

#include <algorithm>
#include <cwctype>

namespace vrperf {

SensorCategory ClassifySensor(const std::string& readingType,
                              const std::string& sensorName,
                              const std::string& label)
{
    (void)label;

    if (readingType == "Temperature" || readingType == "1") {
        if (sensorName.find("GPU") != std::string::npos) {
            return SensorCategory::GpuTemp;
        }
        if (sensorName.find("CPU") != std::string::npos ||
            sensorName.find("Core") != std::string::npos ||
            sensorName.find("Package") != std::string::npos) {
            return SensorCategory::CpuTemp;
        }
        return SensorCategory::CpuTemp;
    }

    if (readingType == "Load" || readingType == "7") {
        if (sensorName.find("GPU") != std::string::npos) {
            return SensorCategory::GpuLoad;
        }
        if (sensorName.find("CPU") != std::string::npos ||
            sensorName.find("Core") != std::string::npos) {
            return SensorCategory::CpuLoad;
        }
        if (sensorName.find("Memory") != std::string::npos) {
            return SensorCategory::RamUsage;
        }
        return SensorCategory::CpuLoad;
    }

    if (readingType == "Clock" || readingType == "6") {
        if (sensorName.find("GPU") != std::string::npos) {
            return SensorCategory::GpuClock;
        }
        return SensorCategory::CpuClock;
    }

    if (readingType == "Fan" || readingType == "3") {
        if (sensorName.find("GPU") != std::string::npos) {
            return SensorCategory::GpuFan;
        }
        return SensorCategory::Fan;
    }

    if (readingType == "Power" || readingType == "5") {
        return SensorCategory::Power;
    }

    if (readingType == "Voltage" || readingType == "2") {
        return SensorCategory::Voltage;
    }

    if (readingType == "Data" || readingType == "13") {
        if (sensorName.find("GPU") != std::string::npos) {
            return SensorCategory::GpuMemory;
        }
        return SensorCategory::RamUsage;
    }

    return SensorCategory::Unknown;
}

namespace {

std::string ReadingTypeToString(HwInfoReadingType type)
{
    switch (type) {
    case SENSOR_TYPE_TEMPERATURE: return "Temperature";
    case SENSOR_TYPE_VOLTAGE: return "Voltage";
    case SENSOR_TYPE_FAN: return "Fan";
    case SENSOR_TYPE_CURRENT: return "Current";
    case SENSOR_TYPE_POWER: return "Power";
    case SENSOR_TYPE_CLOCK: return "Clock";
    case SENSOR_TYPE_LOAD: return "Load";
    case SENSOR_TYPE_DATA: return "Data";
    case SENSOR_TYPE_SMALL_DATA: return "SmallData";
    case SENSOR_TYPE_THROUGHPUT: return "Throughput";
    case SENSOR_TYPE_CONTROL: return "Control";
    case SENSOR_TYPE_LEVEL: return "Level";
    case SENSOR_TYPE_FACTOR: return "Factor";
    default: return "Unknown";
    }
}

std::string WideToNarrow(const wchar_t* text)
{
    if (!text) {
        return {};
    }

    std::string result;
    while (*text) {
        result += static_cast<char>(*text & 0x7F);
        ++text;
    }
    return result;
}

} // namespace

HwInfoReader::HwInfoReader() = default;

HwInfoReader::~HwInfoReader()
{
    Close();
}

bool HwInfoReader::Open()
{
    if (hMapping_) {
        return true;
    }

    hMapping_ = OpenFileMappingW(FILE_MAP_READ, FALSE, L"HWiNFO_SENS_SM2");
    if (!hMapping_) {
        return false;
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
        MapViewOfFile(hMapping_, FILE_MAP_READ, 0, 0, 0));
    if (!pSharedMem_) {
        return false;
    }

    if (pSharedMem_->signature != HWiNFO_SENSORS_SM2_SIGNATURE) {
        UnmapViewOfFile(pSharedMem_);
        pSharedMem_ = nullptr;
        return false;
    }

    return true;
}

bool HwInfoReader::Refresh()
{
    if (!IsConnected() && !Open()) {
        return false;
    }

    if (pSharedMem_->pollTime == lastPollTime_) {
        return false;
    }

    lastPollTime_ = pSharedMem_->pollTime;
    ParseSharedMemory();
    return true;
}

void HwInfoReader::ParseSharedMemory()
{
    if (!pSharedMem_) {
        return;
    }

    const auto* sensors = GetSensorElements(pSharedMem_);
    const auto* readings = GetReadingElements(pSharedMem_);

    std::vector<SensorReading> newReadings;
    newReadings.reserve(pSharedMem_->numReadingElements);

    for (uint32_t i = 0; i < pSharedMem_->numReadingElements; ++i) {
        const auto& re = readings[i];
        if (re.value == 0.0 && re.readingType == SENSOR_TYPE_NONE) {
            continue;
        }

        std::string sensorName;
        std::string sensorLabel;
        for (uint32_t j = 0; j < pSharedMem_->numSensorElements; ++j) {
            if (sensors[j].sensorId == re.sensorId) {
                sensorName = WideToNarrow(sensors[j].sensorName);
                sensorLabel = WideToNarrow(sensors[j].sensorLabel);
                break;
            }
        }

        SensorReading reading;
        reading.readingId = static_cast<int>(i);
        reading.sensorId = static_cast<int>(re.sensorId);
        reading.source = "HWiNFO";
        reading.device = sensorLabel.empty() ? sensorName : sensorLabel;
        reading.value = re.value;
        reading.unit = WideToNarrow(re.unit);
        reading.label = WideToNarrow(re.label);
        reading.category = ClassifySensor(
            ReadingTypeToString(re.readingType),
            sensorName,
            reading.label);

        newReadings.push_back(std::move(reading));
    }

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
