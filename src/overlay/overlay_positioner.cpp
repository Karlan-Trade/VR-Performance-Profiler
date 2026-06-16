#include "overlay/overlay_positioner.h"
#include "overlay/overlay_manager.h"

#include "core/log.h"

#include <cmath>
#include <sstream>

namespace vrperf {

static constexpr float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;
static constexpr float PI = 3.14159265358979323846f;

namespace {

struct Matrix4 {
    float m[4][4] = {};
};

Matrix4 Identity()
{
    Matrix4 matrix;
    for (int i = 0; i < 4; ++i) {
        matrix.m[i][i] = 1.0f;
    }
    return matrix;
}

Matrix4 Multiply(const Matrix4& a, const Matrix4& b)
{
    Matrix4 result;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            for (int k = 0; k < 4; ++k) {
                result.m[row][col] += a.m[row][k] * b.m[k][col];
            }
        }
    }
    return result;
}

Matrix4 Scale(float value)
{
    Matrix4 matrix = Identity();
    matrix.m[0][0] = value;
    matrix.m[1][1] = value;
    matrix.m[2][2] = value;
    return matrix;
}

Matrix4 RotationX(float radians)
{
    Matrix4 matrix = Identity();
    const float c = cosf(radians);
    const float s = sinf(radians);
    matrix.m[1][1] = c;
    matrix.m[1][2] = s;
    matrix.m[2][1] = -s;
    matrix.m[2][2] = c;
    return matrix;
}

Matrix4 RotationY(float radians)
{
    Matrix4 matrix = Identity();
    const float c = cosf(radians);
    const float s = sinf(radians);
    matrix.m[0][0] = c;
    matrix.m[0][2] = -s;
    matrix.m[2][0] = s;
    matrix.m[2][2] = c;
    return matrix;
}

Matrix4 RotationZ(float radians)
{
    Matrix4 matrix = Identity();
    const float c = cosf(radians);
    const float s = sinf(radians);
    matrix.m[0][0] = c;
    matrix.m[0][1] = s;
    matrix.m[1][0] = -s;
    matrix.m[1][1] = c;
    return matrix;
}

Matrix4 Translation(float x, float y, float z)
{
    Matrix4 matrix = Identity();
    matrix.m[3][0] = x;
    matrix.m[3][1] = y;
    matrix.m[3][2] = z;
    return matrix;
}

vr::HmdMatrix34_t ToOpenVrMatrix(const Matrix4& matrix)
{
    vr::HmdMatrix34_t result;
    result.m[0][0] = matrix.m[0][0];
    result.m[0][1] = matrix.m[1][0];
    result.m[0][2] = matrix.m[2][0];
    result.m[0][3] = matrix.m[3][0];
    result.m[1][0] = matrix.m[0][1];
    result.m[1][1] = matrix.m[1][1];
    result.m[1][2] = matrix.m[2][1];
    result.m[1][3] = matrix.m[3][1];
    result.m[2][0] = matrix.m[0][2];
    result.m[2][1] = matrix.m[1][2];
    result.m[2][2] = matrix.m[2][2];
    result.m[2][3] = matrix.m[3][2];
    return result;
}

} // namespace

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

void OverlayPositioner::SetOverlayOffset(float x, float y, float z)
{
    offsetX_ = x;
    offsetY_ = y;
    offsetZ_ = z;
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
    m.m[1][0] = sy * sp;  m.m[1][1] = cp;    m.m[1][2] = cy * sp; m.m[1][3] = -0.3f + offsetY_;
    m.m[2][0] = sy * cp;  m.m[2][1] = -sp;   m.m[2][2] = cy * cp; m.m[2][3] = -hudDistance_ + offsetZ_;
    m.m[0][3] = offsetX_;

    return m;
}

vr::HmdMatrix34_t OverlayPositioner::MakeWristTransform() const
{
    const float x = isLeftHand_ ? -0.07f : 0.07f;
    const float y = -0.05f;
    const float z = 0.06f;
    const float rotX = isLeftHand_ ? PI * 0.5f : -PI * 0.5f;
    const float rotY = isLeftHand_ ? PI * 0.5f : -PI * 0.5f;
    const float rotZ = -PI * 0.5f;

    Matrix4 matrix = Scale(0.25f);
    matrix = Multiply(matrix, RotationX(rotX));
    matrix = Multiply(matrix, RotationY(rotY));
    matrix = Multiply(matrix, RotationZ(rotZ));
    matrix = Multiply(matrix, Translation(x + offsetX_, y + offsetY_, z + offsetZ_));
    return ToOpenVrMatrix(matrix);
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
        const auto err = overlay->SetOverlayTransformTrackedDeviceRelative(
            handle,
            vr::k_unTrackedDeviceIndex_Hmd,
            &transform
        );
        std::ostringstream ss;
        ss << "OpenVR HUD transform result="
           << overlay->GetOverlayErrorNameFromEnum(err)
           << " matrix=["
           << transform.m[0][0] << "," << transform.m[0][1] << ","
           << transform.m[0][2] << "," << transform.m[0][3] << ";"
           << transform.m[1][0] << "," << transform.m[1][1] << ","
           << transform.m[1][2] << "," << transform.m[1][3] << ";"
           << transform.m[2][0] << "," << transform.m[2][1] << ","
           << transform.m[2][2] << "," << transform.m[2][3] << "]";
        LogInfo(ss.str());

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
            const auto err = overlay->SetOverlayTransformTrackedDeviceRelative(
                handle,
                controllerIdx,
                &transform
            );
            std::ostringstream ss;
            ss << "OpenVR Wrist transform result="
               << overlay->GetOverlayErrorNameFromEnum(err)
               << " controller=" << controllerIdx;
            LogInfo(ss.str());
        } else {
            LogInfo("OpenVR Wrist transform skipped: no controller found");
        }
        break;
    }
    }
}

} // namespace vrperf
