#include "data/windows_fallback_provider.h"

#include <Windows.h>
#include <cassert>
#include <iostream>

int main()
{
    vrperf::WindowsFallbackProvider provider;
    provider.Refresh();
    Sleep(20);
    assert(provider.Refresh());

    auto readings = provider.GetReadings();
    assert(!readings.empty());

    bool hasRam = false;
    for (const auto& reading : readings) {
        if (reading.category == vrperf::SensorCategory::RamUsage) {
            hasRam = true;
            assert(reading.value >= 0.0);
            assert(reading.value <= 100.0);
        }
    }

    assert(hasRam);
    std::cout << "[PASS] Windows fallback provider test passed\n";
    return 0;
}
