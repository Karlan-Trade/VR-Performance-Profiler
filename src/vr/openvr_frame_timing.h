#pragma once

#include <cstdint>
#include <openvr.h>

namespace vrperf {

struct OpenVrFrameTimingSnapshot {
    bool available = false;
    double frameTimeMs = 0.0;
    double gpuFrameTimeMs = 0.0;
    uint32_t droppedFrames = 0;
};

class OpenVrFrameTiming {
public:
    OpenVrFrameTimingSnapshot Read() const;
};

} // namespace vrperf
