#include "vr/openvr_frame_timing.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace vrperf {

double OpenVrFrameTiming::CalculateFps(double frameIntervalMs)
{
    if (!std::isfinite(frameIntervalMs) || frameIntervalMs <= 0.0) {
        return 0.0;
    }

    return 1000.0 / frameIntervalMs;
}

double OpenVrFrameTiming::CalculateFpsFromFrameDelta(uint32_t newerFrameIndex,
                                                     double newerTimeSeconds,
                                                     uint32_t olderFrameIndex,
                                                     double olderTimeSeconds)
{
    if (!std::isfinite(newerTimeSeconds) ||
        !std::isfinite(olderTimeSeconds) ||
        newerTimeSeconds <= olderTimeSeconds ||
        newerFrameIndex <= olderFrameIndex) {
        return 0.0;
    }

    return static_cast<double>(newerFrameIndex - olderFrameIndex) /
        (newerTimeSeconds - olderTimeSeconds);
}

double OpenVrFrameTiming::SmoothFps(double currentFps,
                                    double instantFps,
                                    double alpha)
{
    if (!std::isfinite(instantFps) || instantFps <= 0.0) {
        return std::isfinite(currentFps) && currentFps > 0.0 ? currentFps : 0.0;
    }

    if (!std::isfinite(currentFps) || currentFps <= 0.0) {
        return instantFps;
    }

    const double clampedAlpha = (std::max)(0.0, (std::min)(alpha, 1.0));
    return currentFps * (1.0 - clampedAlpha) + instantFps * clampedAlpha;
}

OpenVrFrameTimingSnapshot OpenVrFrameTiming::Read()
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

    double compositorFps = 0.0;
    if (hasLastTiming_) {
        compositorFps = CalculateFpsFromFrameDelta(
            timing.m_nFrameIndex,
            timing.m_flSystemTimeInSeconds,
            lastFrameIndex_,
            lastSystemTimeSeconds_);
    }

    if (compositorFps <= 0.0) {
        std::array<vr::Compositor_FrameTiming, 64> timings = {};
        timings[0].m_nSize = sizeof(vr::Compositor_FrameTiming);
        const uint32_t count = compositor->GetFrameTimings(
            timings.data(),
            static_cast<uint32_t>(timings.size()));
        if (count >= 2) {
            const auto& older = timings[0];
            const auto& newer = timings[count - 1];
            compositorFps = CalculateFpsFromFrameDelta(
                newer.m_nFrameIndex,
                newer.m_flSystemTimeInSeconds,
                older.m_nFrameIndex,
                older.m_flSystemTimeInSeconds);
        }
    }

    lastFrameIndex_ = timing.m_nFrameIndex;
    lastSystemTimeSeconds_ = timing.m_flSystemTimeInSeconds;
    hasLastTiming_ = true;

    snapshot.available = true;
    snapshot.appFrameTimeMs = timing.m_flClientFrameIntervalMs;
    snapshot.fps = compositorFps > 0.0
        ? compositorFps
        : CalculateFps(snapshot.appFrameTimeMs);
    snapshot.frameTimeMs = snapshot.fps > 0.0
        ? 1000.0 / snapshot.fps
        : snapshot.appFrameTimeMs;
    smoothedFps_ = SmoothFps(smoothedFps_, snapshot.fps);
    snapshot.smoothedFps = smoothedFps_;
    snapshot.gpuFrameTimeMs = timing.m_flTotalRenderGpuMs > 0.0f
        ? timing.m_flTotalRenderGpuMs
        : timing.m_flPreSubmitGpuMs
            + timing.m_flPostSubmitGpuMs
            + timing.m_flCompositorRenderGpuMs;
    snapshot.framePresents = timing.m_nNumFramePresents;
    snapshot.droppedFrames = timing.m_nNumDroppedFrames;

    auto* system = vr::VRSystem();
    if (system) {
        vr::ETrackedPropertyError error = vr::TrackedProp_Success;
        const float refreshHz = system->GetFloatTrackedDeviceProperty(
            vr::k_unTrackedDeviceIndex_Hmd,
            vr::Prop_DisplayFrequency_Float,
            &error);
        if (error == vr::TrackedProp_Success && refreshHz > 0.0f) {
            snapshot.refreshHz = refreshHz;
        }
    }

    return snapshot;
}

} // namespace vrperf
