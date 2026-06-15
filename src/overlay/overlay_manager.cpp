#include "overlay/overlay_manager.h"

#include "core/log.h"

#include <iostream>
#include <sstream>

namespace vrperf {

OverlayManager::OverlayManager() = default;

OverlayManager::~OverlayManager()
{
    Shutdown();
}

bool OverlayManager::Initialize()
{
    if (initialized_) return true;

    LogInfo("OpenVR Initialize: calling VR_Init as overlay app");
    vr::EVRInitError error = vr::VRInitError_None;
    system_ = vr::VR_Init(&error, vr::VRApplication_Overlay);

    if (error != vr::VRInitError_None) {
        std::ostringstream ss;
        ss << "OpenVR init failed: "
           << vr::VR_GetVRInitErrorAsEnglishDescription(error);
        LogInfo(ss.str());
        std::cerr << ss.str() << std::endl;
        return false;
    }

    overlay_ = vr::VROverlay();
    if (!overlay_) {
        LogInfo("OpenVR init failed: VROverlay interface is null");
        std::cerr << "Failed to get IVROverlay interface" << std::endl;
        vr::VR_Shutdown();
        system_ = nullptr;
        return false;
    }

    initialized_ = true;
    LogInfo("OpenVR Initialize: success");
    return true;
}

void OverlayManager::Shutdown()
{
    if (overlay_ && handle_) {
        overlay_->DestroyOverlay(handle_);
        handle_ = 0;
    }

    if (system_) {
        vr::VR_Shutdown();
        system_ = nullptr;
        overlay_ = nullptr;
    }

    initialized_ = false;
    overlayVisible_ = false;
}

bool OverlayManager::CreateOverlay(const std::string& key, const std::string& name)
{
    if (!initialized_ || !overlay_) return false;

    LogInfo("OpenVR CreateOverlay: FindOverlay " + key);
    vr::EVROverlayError err = overlay_->FindOverlay(key.c_str(), &handle_);
    if (err == vr::VROverlayError_UnknownOverlay) {
        LogInfo("OpenVR CreateOverlay: not found, creating " + key);
        err = overlay_->CreateOverlay(
            key.c_str(), name.c_str(), &handle_
        );
    } else {
        std::ostringstream ss;
        ss << "OpenVR CreateOverlay: FindOverlay result="
           << overlay_->GetOverlayErrorNameFromEnum(err)
           << " handle=" << handle_;
        LogInfo(ss.str());
    }

    if (err != vr::VROverlayError_None) {
        std::ostringstream ss;
        ss << "CreateOverlay failed: "
           << overlay_->GetOverlayErrorNameFromEnum(err);
        LogInfo(ss.str());
        std::cerr << ss.str() << std::endl;
        handle_ = 0;
        return false;
    }

    // Default properties
    overlay_->SetOverlayWidthInMeters(handle_, 1.0f);
    overlay_->SetOverlayAlpha(handle_, 0.85f);
    overlay_->SetOverlayCurvature(handle_, 0.0f);
    overlay_->SetOverlaySortOrder(handle_, 100);
    overlay_->SetOverlayColor(handle_, 1.0f, 1.0f, 1.0f);
    overlay_->SetOverlayFlag(handle_, vr::VROverlayFlags_IsPremultiplied, true);
    overlay_->SetOverlayFlag(handle_, vr::VROverlayFlags_IgnoreTextureAlpha, true);
    overlay_->SetOverlayFlag(handle_, vr::VROverlayFlags_SortWithNonSceneOverlays, true);

    std::ostringstream ss;
    ss << "OpenVR CreateOverlay: success handle=" << handle_;
    LogInfo(ss.str());

    return true;
}

void OverlayManager::SetWidthMeters(float widthMeters)
{
    if (overlay_ && handle_) {
        const auto err = overlay_->SetOverlayWidthInMeters(handle_, widthMeters);
        std::ostringstream ss;
        ss << "OpenVR SetOverlayWidthInMeters width=" << widthMeters
           << " result=" << overlay_->GetOverlayErrorNameFromEnum(err);
        LogInfo(ss.str());
    }
}

void OverlayManager::SetAlpha(float alpha)
{
    if (overlay_ && handle_) {
        const auto err = overlay_->SetOverlayAlpha(handle_, alpha);
        std::ostringstream ss;
        ss << "OpenVR SetOverlayAlpha alpha=" << alpha
           << " result=" << overlay_->GetOverlayErrorNameFromEnum(err);
        LogInfo(ss.str());
    }
}

void OverlayManager::SetInputNone()
{
    if (overlay_ && handle_) {
        const auto err = overlay_->SetOverlayInputMethod(
            handle_,
            vr::VROverlayInputMethod_None);
        std::ostringstream ss;
        ss << "OpenVR SetOverlayInputMethod None result="
           << overlay_->GetOverlayErrorNameFromEnum(err);
        LogInfo(ss.str());
    }
}

void OverlayManager::Show()
{
    if (overlay_ && handle_) {
        const auto err = overlay_->ShowOverlay(handle_);
        if (err == vr::VROverlayError_None) {
            overlayVisible_ = true;
            LogInfo("OpenVR ShowOverlay: success");
        } else {
            std::ostringstream ss;
            ss << "ShowOverlay failed: "
               << overlay_->GetOverlayErrorNameFromEnum(err);
            LogInfo(ss.str());
            std::cerr << ss.str() << std::endl;
        }
    }
}

void OverlayManager::Hide()
{
    if (overlay_ && handle_) {
        const auto err = overlay_->HideOverlay(handle_);
        if (err == vr::VROverlayError_None) {
            overlayVisible_ = false;
            LogInfo("OpenVR HideOverlay: success");
        } else {
            std::ostringstream ss;
            ss << "HideOverlay failed: "
               << overlay_->GetOverlayErrorNameFromEnum(err);
            LogInfo(ss.str());
            std::cerr << ss.str() << std::endl;
        }
    }
}

void OverlayManager::ToggleVisibility()
{
    if (overlayVisible_) {
        Hide();
    } else {
        Show();
    }
}

bool OverlayManager::SetTexture(ID3D11Texture2D* d3dTexture)
{
    if (!overlay_ || !handle_ || !d3dTexture) return false;

    vr::Texture_t texture;
    texture.handle = d3dTexture;
    texture.eType = vr::TextureType_DirectX;
    texture.eColorSpace = vr::ColorSpace_Auto;

    vr::VRTextureBounds_t bounds;
    bounds.uMin = 0.0f;
    bounds.uMax = 1.0f;
    bounds.vMin = 0.0f;
    bounds.vMax = 1.0f;
    const auto boundsErr = overlay_->SetOverlayTextureBounds(handle_, &bounds);
    if (boundsErr != vr::VROverlayError_None) {
        std::ostringstream ss;
        ss << "SetOverlayTextureBounds failed: "
           << overlay_->GetOverlayErrorNameFromEnum(boundsErr);
        LogInfo(ss.str());
    }

    const auto err = overlay_->SetOverlayTexture(handle_, &texture);
    if (err != vr::VROverlayError_None) {
        std::ostringstream ss;
        ss << "SetOverlayTexture failed: "
           << overlay_->GetOverlayErrorNameFromEnum(err)
           << " texture=" << d3dTexture;
        LogInfo(ss.str());
        std::cerr << ss.str() << std::endl;
        return false;
    }

    std::ostringstream ss;
    ss << "OpenVR SetOverlayTexture: success texture=" << d3dTexture
       << " visible=" << overlay_->IsOverlayVisible(handle_);
    LogInfo(ss.str());
    return true;
}

void OverlayManager::PollEvents()
{
    if (!overlay_ || !handle_) return;

    vr::VREvent_t event;
    while (overlay_->PollNextOverlayEvent(handle_, &event, sizeof(event))) {
        switch (event.eventType) {
        case vr::VREvent_Quit:
            vr::VR_Shutdown();
            system_ = nullptr;
            overlay_ = nullptr;
            handle_ = 0;
            initialized_ = false;
            overlayVisible_ = false;
            break;
        }
    }
}

} // namespace vrperf
