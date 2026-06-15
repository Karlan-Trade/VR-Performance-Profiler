#include "data/libre_hardware_monitor_bridge_provider.h"

#include "data/metric_aggregator.h"

#include <Windows.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <utility>

namespace vrperf {

namespace {

std::string JsonStringValue(const nlohmann::json& object,
                            const char* key,
                            const std::string& fallback)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string()) {
        return fallback;
    }
    return it->get<std::string>();
}

} // namespace

LibreHardwareMonitorBridgeProvider::LibreHardwareMonitorBridgeProvider()
    : LibreHardwareMonitorBridgeProvider(GetDefaultSnapshotPath())
{
}

LibreHardwareMonitorBridgeProvider::LibreHardwareMonitorBridgeProvider(
    std::filesystem::path snapshotPath,
    std::chrono::milliseconds maxAge)
    : snapshotPath_(std::move(snapshotPath)),
      maxAge_(maxAge)
{
}

bool LibreHardwareMonitorBridgeProvider::Refresh()
{
    readings_.clear();
    available_ = false;

    std::error_code ec;
    if (!std::filesystem::exists(snapshotPath_, ec)) {
        return false;
    }

    const auto lastWrite = std::filesystem::last_write_time(snapshotPath_, ec);
    if (ec) {
        return false;
    }

    const auto now = std::filesystem::file_time_type::clock::now();
    if (now - lastWrite > maxAge_) {
        return false;
    }

    std::ifstream file(snapshotPath_);
    if (!file.is_open()) {
        return false;
    }

    try {
        const auto root = nlohmann::json::parse(file);
        const nlohmann::json* readingsJson = nullptr;
        if (root.is_array()) {
            readingsJson = &root;
        } else if (root.is_object()) {
            const auto it = root.find("readings");
            if (it != root.end() && it->is_array()) {
                readingsJson = &(*it);
            }
        }

        if (!readingsJson) {
            return false;
        }

        for (const auto& item : *readingsJson) {
            if (!item.is_object()) {
                continue;
            }

            const auto category = CategoryFromString(JsonStringValue(item, "category", ""));
            if (category == SensorCategory::Unknown) {
                continue;
            }

            SensorReading reading;
            reading.category = category;
            reading.source = "LibreHardwareMonitor";
            reading.device = JsonStringValue(item, "device", "");
            reading.label = JsonStringValue(item, "label", MetricAggregator::CategoryKey(category));
            reading.value = item.value("value", 0.0);
            reading.unit = JsonStringValue(item, "unit", "");
            readings_.push_back(reading);
        }
    } catch (const std::exception&) {
        readings_.clear();
        return false;
    }

    available_ = !readings_.empty();
    return available_;
}

bool LibreHardwareMonitorBridgeProvider::IsAvailable() const
{
    return available_;
}

std::vector<SensorReading> LibreHardwareMonitorBridgeProvider::GetReadings() const
{
    return readings_;
}

std::filesystem::path LibreHardwareMonitorBridgeProvider::GetDefaultSnapshotPath()
{
    wchar_t localAppData[MAX_PATH] = {};
    const auto len = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return std::filesystem::path(L"lhm-sensors.json");
    }

    return std::filesystem::path(localAppData)
        / L"VRPerfProfiler"
        / L"lhm-sensors.json";
}

SensorCategory LibreHardwareMonitorBridgeProvider::CategoryFromString(
    const std::string& category)
{
    if (category == "cpu_load") return SensorCategory::CpuLoad;
    if (category == "cpu_temp") return SensorCategory::CpuTemp;
    if (category == "cpu_clock") return SensorCategory::CpuClock;
    if (category == "gpu_load") return SensorCategory::GpuLoad;
    if (category == "gpu_temp") return SensorCategory::GpuTemp;
    if (category == "gpu_clock") return SensorCategory::GpuClock;
    if (category == "gpu_memory") return SensorCategory::GpuMemory;
    if (category == "gpu_fan") return SensorCategory::GpuFan;
    if (category == "ram_usage") return SensorCategory::RamUsage;
    if (category == "fan") return SensorCategory::Fan;
    if (category == "voltage") return SensorCategory::Voltage;
    if (category == "power") return SensorCategory::Power;
    return SensorCategory::Unknown;
}

} // namespace vrperf
