#include "vr/openvr_frame_timing.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

bool NearlyEqual(double a, double b, double epsilon = 0.001)
{
    return std::fabs(a - b) < epsilon;
}

} // namespace

int main()
{
    assert(NearlyEqual(vrperf::OpenVrFrameTiming::CalculateFps(11.111), 90.0009));
    assert(NearlyEqual(vrperf::OpenVrFrameTiming::CalculateFps(13.8889), 72.0));
    assert(vrperf::OpenVrFrameTiming::CalculateFps(0.0) == 0.0);
    assert(vrperf::OpenVrFrameTiming::CalculateFps(-1.0) == 0.0);

    assert(NearlyEqual(
        vrperf::OpenVrFrameTiming::CalculateFpsFromFrameDelta(190, 12.0, 100, 11.0),
        90.0));
    assert(vrperf::OpenVrFrameTiming::CalculateFpsFromFrameDelta(100, 12.0, 100, 11.0) == 0.0);
    assert(vrperf::OpenVrFrameTiming::CalculateFpsFromFrameDelta(190, 11.0, 100, 12.0) == 0.0);

    assert(NearlyEqual(vrperf::OpenVrFrameTiming::SmoothFps(0.0, 90.0), 90.0));
    assert(NearlyEqual(vrperf::OpenVrFrameTiming::SmoothFps(90.0, 80.0, 0.15), 88.5));
    assert(NearlyEqual(vrperf::OpenVrFrameTiming::SmoothFps(90.0, 80.0, 2.0), 80.0));
    assert(NearlyEqual(vrperf::OpenVrFrameTiming::SmoothFps(90.0, 80.0, -1.0), 90.0));
    assert(NearlyEqual(vrperf::OpenVrFrameTiming::SmoothFps(90.0, 0.0), 90.0));

    std::cout << "[PASS] OpenVrFrameTiming tests passed\n";
    return 0;
}
