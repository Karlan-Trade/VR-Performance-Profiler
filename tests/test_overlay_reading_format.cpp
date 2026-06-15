#include "render/d2d_renderer.h"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace vrperf;

namespace {

SensorReading Reading(SensorCategory category, double value, std::string unit = {})
{
    SensorReading reading;
    reading.category = category;
    reading.value = value;
    reading.unit = std::move(unit);
    return reading;
}

SensorReading ReadingWithLabel(std::string device, std::string label)
{
    SensorReading reading;
    reading.category = SensorCategory::GpuTemp;
    reading.device = std::move(device);
    reading.label = std::move(label);
    return reading;
}

} // namespace

int main()
{
    {
        const auto value = FormatOverlayReadingValue(
            Reading(SensorCategory::GpuTemp, 46.0));
        assert(value == L"46\u00B0C");
        assert(value.size() == 4);
        assert(value[2] == L'\u00B0');
    }

    {
        std::string utf8Unit;
        utf8Unit.push_back(static_cast<char>(0xC2));
        utf8Unit.push_back(static_cast<char>(0xB0));
        utf8Unit.push_back('C');

        const auto value = FormatOverlayReadingValue(
            Reading(SensorCategory::Voltage, 1.25, utf8Unit));
        assert(value == L"1.25 V");

        const auto fallbackValue = FormatOverlayReadingValue(
            Reading(SensorCategory::Unknown, 46.0, utf8Unit));
        assert(fallbackValue == L"46.0 \u00B0C");
    }

    {
        const auto value = FormatOverlayReadingValue(
            Reading(SensorCategory::CpuTemp, std::nan("")));
        assert(value == L"--");
    }

    {
        assert(FormatOverlayReadingLabel(
                   ReadingWithLabel("GPU2", "GPU2 GPU temperature")) ==
               L"GPU temperature");
        assert(FormatOverlayReadingLabel(
                   ReadingWithLabel("", "GPU2 GPU2 temperature")) ==
               L"GPU2 temperature");
        assert(FormatOverlayReadingLabel(
                   ReadingWithLabel("GPU2", "GPU2 GPU2 temperature")) ==
               L"GPU2 temperature");
        assert(FormatOverlayReadingLabel(
                   ReadingWithLabel("GPU2", "GPU2")) ==
               L"GPU2");
        assert(FormatOverlayReadingLabel(
                   ReadingWithLabel("", "CPU temperature")) ==
               L"CPU temperature");
    }

    std::cout << "[PASS] Overlay reading formatting tests\n";
    return 0;
}
