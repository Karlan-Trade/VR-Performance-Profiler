#include "overlay/overlay_positioner.h"
#include "overlay/overlay_manager.h"

#include <cmath>

namespace vrperf {

static constexpr float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;

OverlayPositioner::OverlayPositioner() = default;

void OverlayPositioner::SetMode(OverlayMode mode)
{
    mode_ = mode;
}

void OverlayPositioner::ToggleMode()
{
    mode_ = (mode_ == OverlayMode::HUD) ? OverlayMode::Wrist : OverlayMode::HUD;
}

void OverlayPositioner::SetHudPosition(float yawDeg, float pitchDeg, float distance)
{
    hudYaw_ = yawDeg;
    hudPitch_ = pitchDeg;
    hudDistance_ = distance;
}

void OverlayPositioner::SetWristHand(bool isLeft)
{
    isLeftHand_ = isLeft;
}

vr::HmdMatrix34_t OverlayPositioner::MakeHudTransform() const
{
    float yaw = hudYaw_ * DEG_TO_RAD;
    float pitch = hudPitch_ * DEG_TO_RAD;

    float cy = cosf(yaw),   sy = sinf(yaw);
    float cp = cosf(pitch), sp = sinf(pitch);

    // Rotation: pitch around X, then yaw around Y
    // Translation: -Z is forward in OpenVR
    vr::HmdMatrix34_t m;
    m.m[0][0] = cy;      m.m[0][1] = 0;     m.m[0][2] = -sy;     m.m[0][3] = 0;
    m.m[1][0] = sy * sp;  m.m[1][1] = cp;    m.m[1][2] = cy * sp; m.m[1][3] = 0;
    m.m[2][0] = sy * cp;  m.m[2][1] = -sp;   m.m[2][2] = cy * cp; m.m[2][3] = -hudDistance_;

    return m;
}

vr::HmdMatrix34_t OverlayPositioner::MakeWristTransform() const
{
    // Position overlay relative to controller (like a wristwatch)
    vr::HmdMatrix34_t m;
    // Identity rotation
    m.m[0][0] = 1.0f; m.m[0][1] = 0.0f; m.m[0][2] = 0.0f;
    m.m[1][0] = 0.0f; m.m[1][1] = 1.0f; m.m[1][2] = 0.0f;
    m.m[2][0] = 0.0f; m.m[2][1] = 0.0f; m.m[2][2] = 1.0f;

    // Offset: slightly above and in front of the wrist
    float xOff = isLeftHand_ ? 0.05f : -0.05f;
    m.m[0][3] = xOff + wristOffsetX_;
    m.m[1][3] = wristOffsetY_;
    m.m[2][3] = wristOffsetZ_;

    return m;
}

void OverlayPositioner::ApplyTransform(OverlayManager* overlayMgr)
{
    if (!overlayMgr || !overlayMgr->IsInitialized()) return;

    auto* overlay = overlayMgr->GetOverlay();
    auto handle = overlayMgr->GetHandle();
    if (!overlay || !handle) return;

    switch (mode_) {
    case OverlayMode::HUD: {
        // Apply transform relative to HMD
        vr::HmdMatrix34_t transform = MakeHudTransform();
        overlay->SetOverlayTransformTrackedDeviceRelative(
            handle,
            vr::k_unTrackedDeviceIndex_Hmd,
            &transform
        );

        break;
    }
    case OverlayMode::Wrist: {
        // Find the requested controller. Fall back to the first controller
        // if SteamVR has not assigned left/right roles yet.
        vr::TrackedDeviceIndex_t controllerIdx = vr::k_unTrackedDeviceIndexInvalid;
        auto* system = vr::VRSystem();
        if (system) {
            const auto desiredRole = isLeftHand_
                ? vr::TrackedControllerRole_LeftHand
                : vr::TrackedControllerRole_RightHand;

            for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i) {
                if (system->GetTrackedDeviceClass(i) != vr::TrackedDeviceClass_Controller) {
                    continue;
                }

                if (controllerIdx == vr::k_unTrackedDeviceIndexInvalid) {
                    controllerIdx = i;
                }

                if (system->GetControllerRoleForTrackedDeviceIndex(i) == desiredRole) {
                    controllerIdx = i;
                    break;
                }
            }
        }

        if (controllerIdx != vr::k_unTrackedDeviceIndexInvalid) {
            vr::HmdMatrix34_t transform = MakeWristTransform();
            overlay->SetOverlayTransformTrackedDeviceRelative(
                handle,
                controllerIdx,
                &transform
            );
        }
        break;
    }
    }
}

} // namespace vrperf
