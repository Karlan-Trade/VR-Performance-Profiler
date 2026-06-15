#pragma once

#include <cstdint>
#include <openvr.h>

namespace vrperf {

struct OpenVrFrameTimingSnapshot {
    bool available = false;
    double fps = 0.0;
    double smoothedFps = 0.0;
    double refreshHz = 0.0;
    double frameTimeMs = 0.0;
    double appFrameTimeMs = 0.0;
    double gpuFrameTimeMs = 0.0;
    uint32_t framePresents = 0;
    uint32_t droppedFrames = 0;
};

class OpenVrFrameTiming {
public:
    OpenVrFrameTimingSnapshot Read();

    static double CalculateFps(double frameIntervalMs);
    static double CalculateFpsFromFrameDelta(uint32_t newerFrameIndex,
                                             double newerTimeSeconds,
                                             uint32_t olderFrameIndex,
                                             double olderTimeSeconds);
    static double SmoothFps(double currentFps,
                            double instantFps,
                            double alpha = 0.15);

private:
    double smoothedFps_ = 0.0;
    bool hasLastTiming_ = false;
    uint32_t lastFrameIndex_ = 0;
    double lastSystemTimeSeconds_ = 0.0;
};

} // namespace vrperf
