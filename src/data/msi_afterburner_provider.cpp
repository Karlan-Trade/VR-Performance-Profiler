#include "data/msi_afterburner_provider.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <string>
#include <utility>

namespace vrperf {

namespace {

constexpr wchar_t kDefaultMappingName[] = L"MAHMSharedMemory";
constexpr uint32_t kMaxReasonableEntries = 4096;
constexpr uint32_t kMaxReasonableGpuEntries = 64;

bool IsGpuTemperatureSource(uint32_t sourceId)
{
    return sourceId == mahm::kSourceGpuTemperature ||
           sourceId == mahm::kSourcePcbTemperature ||
           sourceId == mahm::kSourceMemTemperature ||
           sourceId == mahm::kSourceVrmTemperature ||
           sourceId == mahm::kSourceGpuTemperature2 ||
           sourceId == mahm::kSourcePcbTemperature2 ||
           sourceId == mahm::kSourceMemTemperature2 ||
           sourceId == mahm::kSourceVrmTemperature2 ||
           sourceId == mahm::kSourceGpuTemperature3 ||
           sourceId == mahm::kSourcePcbTemperature3 ||
           sourceId == mahm::kSourceMemTemperature3 ||
           sourceId == mahm::kSourceVrmTemperature3 ||
           sourceId == mahm::kSourceGpuTemperature4 ||
           sourceId == mahm::kSourcePcbTemperature4 ||
           sourceId == mahm::kSourceMemTemperature4 ||
           sourceId == mahm::kSourceVrmTemperature4 ||
           sourceId == mahm::kSourceGpuTemperature5 ||
           sourceId == mahm::kSourcePcbTemperature5 ||
           sourceId == mahm::kSourceMemTemperature5 ||
           sourceId == mahm::kSourceVrmTemperature5;
}

bool IsGpuFanSource(uint32_t sourceId)
{
    return sourceId == mahm::kSourceFanSpeed ||
           sourceId == mahm::kSourceFanTachometer ||
           sourceId == mahm::kSourceFanSpeed2 ||
           sourceId == mahm::kSourceFanTachometer2 ||
           sourceId == mahm::kSourceFanSpeed3 ||
           sourceId == mahm::kSourceFanTachometer3;
}

bool IsPowerSource(uint32_t sourceId)
{
    return sourceId == mahm::kSourceGpuRelPower ||
           sourceId == mahm::kSourceGpuAbsPower ||
           sourceId == mahm::kSourceCpuPower;
}

bool IsCpuSource(uint32_t sourceId)
{
    return sourceId == mahm::kSourceCpuTemperature ||
           sourceId == mahm::kSourceCpuUsage ||
           sourceId == mahm::kSourceCpuClock ||
           sourceId == mahm::kSourceCpuPower;
}

bool IsGpuAttributedSource(uint32_t sourceId)
{
    return !IsCpuSource(sourceId) &&
           sourceId != mahm::kSourceRamUsage;
}

bool IsNumberedCpuLabel(const std::string& label)
{
    if (label.size() < 5 ||
        label[0] != 'C' ||
        label[1] != 'P' ||
        label[2] != 'U') {
        return false;
    }

    size_t index = 3;
    bool sawDigit = false;
    while (index < label.size() &&
           label[index] >= '0' &&
           label[index] <= '9') {
        sawDigit = true;
        ++index;
    }

    return sawDigit &&
           index < label.size() &&
           (label[index] == ' ' || label[index] == '\t');
}

} // namespace

MsiAfterburnerProvider::MsiAfterburnerProvider()
    : MsiAfterburnerProvider(kDefaultMappingName)
{
}

MsiAfterburnerProvider::MsiAfterburnerProvider(std::wstring mappingName)
    : mappingName_(std::move(mappingName))
{
}

MsiAfterburnerProvider::~MsiAfterburnerProvider()
{
    Close();
}

bool MsiAfterburnerProvider::Refresh()
{
    readings_.clear();
    available_ = false;

    if (!view_ && !Open()) {
        return false;
    }

    const auto* header = reinterpret_cast<const mahm::SharedMemoryHeader*>(view_);
    if (header->signature == mahm::kSignatureDead) {
        Close();
        return false;
    }

    if (!IsHeaderUsable(*header)) {
        return false;
    }

    const auto gpuNames = ReadGpuNames(*header);
    std::vector<SensorReading> readings;
    readings.reserve(header->numEntries);

    for (uint32_t index = 0; index < header->numEntries; ++index) {
        const auto* entry = reinterpret_cast<const mahm::SharedMemoryEntry*>(
            view_ + header->headerSize + index * header->entrySize);

        const auto category = CategoryFromSourceId(entry->sourceId);
        const auto value = static_cast<double>(entry->data);
        if (!IsValueUsable(category, value)) {
            continue;
        }

        const auto label = LabelForEntry(*entry);
        if (IsCpuSource(entry->sourceId) && IsNumberedCpuLabel(label)) {
            continue;
        }

        SensorReading reading;
        reading.category = category;
        reading.source = "MSI Afterburner";
        if (IsGpuAttributedSource(entry->sourceId) &&
            entry->gpu != mahm::kUnknownGpu &&
            entry->gpu < gpuNames.size()) {
            reading.device = gpuNames[entry->gpu];
        }
        reading.label = label;
        reading.value = value;
        reading.unit = UnitForEntry(*entry);
        reading.sensorId = static_cast<int>(
            entry->gpu == mahm::kUnknownGpu ? -1 : entry->gpu);
        reading.readingId = static_cast<int>(entry->sourceId);
        readings.push_back(std::move(reading));
    }

    readings_ = std::move(readings);
    available_ = !readings_.empty();
    return available_;
}

bool MsiAfterburnerProvider::IsAvailable() const
{
    return available_;
}

std::vector<SensorReading> MsiAfterburnerProvider::GetReadings() const
{
    return readings_;
}

void MsiAfterburnerProvider::Close()
{
    if (view_) {
        UnmapViewOfFile(view_);
        view_ = nullptr;
    }

    if (mapping_) {
        CloseHandle(mapping_);
        mapping_ = nullptr;
    }

    available_ = false;
}

SensorCategory MsiAfterburnerProvider::CategoryFromSourceId(uint32_t sourceId)
{
    if (IsGpuTemperatureSource(sourceId)) return SensorCategory::GpuTemp;
    if (IsGpuFanSource(sourceId)) return SensorCategory::GpuFan;

    switch (sourceId) {
    case mahm::kSourceCoreClock:
    case mahm::kSourceShaderClock:
    case mahm::kSourceMemoryClock:
        return SensorCategory::GpuClock;
    case mahm::kSourceGpuUsage:
        return SensorCategory::GpuLoad;
    case mahm::kSourceMemoryUsage:
    case mahm::kSourceMemoryUsageProcess:
        return SensorCategory::GpuMemory;
    case mahm::kSourceFbUsage:
    case mahm::kSourceVidUsage:
    case mahm::kSourceBusUsage:
        return SensorCategory::Unknown;
    case mahm::kSourceGpuVoltage:
        return SensorCategory::Voltage;
    case mahm::kSourceCpuTemperature:
        return SensorCategory::CpuTemp;
    case mahm::kSourceCpuUsage:
        return SensorCategory::CpuLoad;
    case mahm::kSourceRamUsage:
        return SensorCategory::RamUsage;
    case mahm::kSourceCpuClock:
        return SensorCategory::CpuClock;
    default:
        if (IsPowerSource(sourceId)) {
            return SensorCategory::Power;
        }
        return SensorCategory::Unknown;
    }
}

bool MsiAfterburnerProvider::Open()
{
    mapping_ = OpenFileMappingW(FILE_MAP_READ, FALSE, mappingName_.c_str());
    if (!mapping_) {
        return false;
    }

    view_ = static_cast<const uint8_t*>(
        MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, 0));
    if (!view_) {
        Close();
        return false;
    }

    return true;
}

bool MsiAfterburnerProvider::IsHeaderUsable(
    const mahm::SharedMemoryHeader& header) const
{
    return header.signature == mahm::kSignatureValid &&
           header.version >= mahm::kMinSupportedVersion &&
           header.headerSize >= sizeof(mahm::SharedMemoryHeader) &&
           header.entrySize >= sizeof(mahm::SharedMemoryEntry) &&
           (header.numGpuEntries == 0 ||
            header.gpuEntrySize >= sizeof(mahm::SharedMemoryGpuEntry)) &&
           header.numEntries <= kMaxReasonableEntries &&
           header.numGpuEntries <= kMaxReasonableGpuEntries;
}

bool MsiAfterburnerProvider::IsValueUsable(SensorCategory category, double value)
{
    if (!std::isfinite(value) ||
        value == static_cast<double>(FLT_MAX)) {
        return false;
    }

    switch (category) {
    case SensorCategory::CpuTemp:
    case SensorCategory::GpuTemp:
    case SensorCategory::CpuClock:
    case SensorCategory::GpuClock:
    case SensorCategory::Voltage:
    case SensorCategory::Power:
        return value > 0.0;
    default:
        return true;
    }
}

std::string MsiAfterburnerProvider::StringFromBuffer(
    const char* text,
    size_t capacity)
{
    return std::string(text, strnlen_s(text, capacity));
}

std::string MsiAfterburnerProvider::LabelForEntry(
    const mahm::SharedMemoryEntry& entry)
{
    return StringFromBuffer(entry.sourceName, MAX_PATH);
}

std::string MsiAfterburnerProvider::UnitForEntry(
    const mahm::SharedMemoryEntry& entry)
{
    return StringFromBuffer(entry.sourceUnits, MAX_PATH);
}

std::vector<std::string> MsiAfterburnerProvider::ReadGpuNames(
    const mahm::SharedMemoryHeader& header) const
{
    std::vector<std::string> names;
    names.reserve(header.numGpuEntries);

    const auto* gpuBase = view_ + header.headerSize +
        header.numEntries * header.entrySize;

    for (uint32_t index = 0; index < header.numGpuEntries; ++index) {
        const auto* entry = reinterpret_cast<const mahm::SharedMemoryGpuEntry*>(
            gpuBase + index * header.gpuEntrySize);

        auto device = StringFromBuffer(entry->device, MAX_PATH);
        if (device.empty()) {
            device = StringFromBuffer(entry->family, MAX_PATH);
        }
        if (device.empty()) {
            device = StringFromBuffer(entry->gpuId, MAX_PATH);
        }

        if (device.empty()) {
            device = "Unknown GPU";
        }

        names.push_back("GPU" + std::to_string(index + 1) + ": " + device);
    }

    return names;
}

} // namespace vrperf
