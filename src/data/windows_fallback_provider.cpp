#include "data/windows_fallback_provider.h"

namespace vrperf {

unsigned long long WindowsFallbackProvider::ToUInt64(const FILETIME& ft)
{
    return (static_cast<unsigned long long>(ft.dwHighDateTime) << 32)
         | static_cast<unsigned long long>(ft.dwLowDateTime);
}

bool WindowsFallbackProvider::Refresh()
{
    readings_.clear();

    FILETIME idleTime;
    FILETIME kernelTime;
    FILETIME userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        if (hasPreviousCpuTimes_) {
            const auto idle = ToUInt64(idleTime) - ToUInt64(previousIdleTime_);
            const auto kernel = ToUInt64(kernelTime) - ToUInt64(previousKernelTime_);
            const auto user = ToUInt64(userTime) - ToUInt64(previousUserTime_);
            const auto total = kernel + user;
            if (total > 0) {
                SensorReading cpu;
                cpu.category = SensorCategory::CpuLoad;
                cpu.source = "Windows";
                cpu.label = "CPU Load";
                cpu.value = 100.0 * static_cast<double>(total - idle) /
                            static_cast<double>(total);
                cpu.unit = "%";
                readings_.push_back(cpu);
            }
        }

        previousIdleTime_ = idleTime;
        previousKernelTime_ = kernelTime;
        previousUserTime_ = userTime;
        hasPreviousCpuTimes_ = true;
    }

    MEMORYSTATUSEX mem = {};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        SensorReading ram;
        ram.category = SensorCategory::RamUsage;
        ram.source = "Windows";
        ram.label = "RAM";
        ram.value = static_cast<double>(mem.dwMemoryLoad);
        ram.unit = "%";
        readings_.push_back(ram);
    }

    return !readings_.empty();
}

std::vector<SensorReading> WindowsFallbackProvider::GetReadings() const
{
    return readings_;
}

} // namespace vrperf
