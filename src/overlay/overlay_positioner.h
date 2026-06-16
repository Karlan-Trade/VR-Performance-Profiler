#pragma once

#include <openvr.h>

namespace vrperf {

class OverlayManager;

enum class OverlayMode {
    HUD,
    Wrist
};

class OverlayPositioner {
public:
    OverlayPositioner();

    // Set overlay mode
    void SetMode(OverlayMode mode);
    OverlayMode GetMode() const { return mode_; }
    void ToggleMode();

    // HUD position (angles in degrees, distance in meters)
    void SetHudPosition(float yawDeg, float pitchDeg, float distance);
    void SetOverlayOffset(float x, float y, float z);

    // Wrist settings
    void SetWristHand(bool isLeft);
    bool IsLeftHand() const { return isLeftHand_; }

    // Apply current transform to overlay
    void ApplyTransform(OverlayManager* overlayMgr);

    vr::HmdMatrix34_t MakeHudTransform() const;
    vr::HmdMatrix34_t MakeWristTransform() const;

private:
    OverlayMode mode_ = OverlayMode::HUD;

    // HUD parameters
    float hudYaw_ = 0.0f;
    float hudPitch_ = -15.0f;
    float hudDistance_ = 1.0f;
    float offsetX_ = 0.0f;
    float offsetY_ = 0.0f;
    float offsetZ_ = 0.0f;

    bool isLeftHand_ = true;
};

} // namespace vrperf
