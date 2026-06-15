#include "vr/openvr_frame_timing.h"

namespace vrperf {

OpenVrFrameTimingSnapshot OpenVrFrameTiming::Read() const
{
    OpenVrFrameTimingSnapshot snapshot;
    auto* compositor = vr::VRCompositor();
    if (!compositor) {
        return snapshot;
    }

    vr::Compositor_FrameTiming timing = {};
    timing.m_nSize = sizeof(timing);
    if (!compositor->GetFrameTiming(&timing, 0)) {
        return snapshot;
    }

    snapshot.available = true;
    snapshot.frameTimeMs = timing.m_flSystemTimeInSeconds * 1000.0;
    snapshot.gpuFrameTimeMs = timing.m_flPreSubmitGpuMs
                            + timing.m_flPostSubmitGpuMs
                            + timing.m_flCompositorRenderGpuMs;
    snapshot.droppedFrames = timing.m_nNumDroppedFrames;
    return snapshot;
}

} // namespace vrperf
