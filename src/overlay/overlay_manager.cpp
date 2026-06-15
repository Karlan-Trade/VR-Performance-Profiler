#include "overlay/overlay_manager.h"
#include <iostream>

namespace vrperf {

OverlayManager::OverlayManager() = default;

OverlayManager::~OverlayManager()
{
    Shutdown();
}

bool OverlayManager::Initialize()
{
    if (initialized_) return true;

    vr::EVRInitError error = vr::VRInitError_None;
    system_ = vr::VR_Init(&error, vr::VRApplication_Overlay);

    if (error != vr::VRInitError_None) {
        std::cerr << "OpenVR init failed: "
                  << vr::VR_GetVRInitErrorAsEnglishDescription(error) << std::endl;
        return false;
    }

    overlay_ = vr::VROverlay();
    if (!overlay_) {
        std::cerr << "Failed to get IVROverlay interface" << std::endl;
        vr::VR_Shutdown();
        system_ = nullptr;
        return false;
    }

    initialized_ = true;
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

    vr::EVROverlayError err = overlay_->CreateOverlay(
        key.c_str(), name.c_str(), &handle_
    );

    if (err != vr::VROverlayError_None) {
        std::cerr << "CreateOverlay failed: " << overlay_->GetOverlayErrorNameFromEnum(err) << std::endl;
        return false;
    }

    // Default properties
    overlay_->SetOverlayWidthInMeters(handle_, 0.3f);
    overlay_->SetOverlayAlpha(handle_, 0.85f);
    overlay_->SetOverlayCurvature(handle_, 0.0f);

    return true;
}

void OverlayManager::SetWidthMeters(float widthMeters)
{
    if (overlay_ && handle_) {
        overlay_->SetOverlayWidthInMeters(handle_, widthMeters);
    }
}

void OverlayManager::SetAlpha(float alpha)
{
    if (overlay_ && handle_) {
        overlay_->SetOverlayAlpha(handle_, alpha);
    }
}

void OverlayManager::SetInputNone()
{
    if (overlay_ && handle_) {
        overlay_->SetOverlayInputMethod(handle_, vr::VROverlayInputMethod_None);
    }
}

void OverlayManager::Show()
{
    if (overlay_ && handle_) {
        overlay_->ShowOverlay(handle_);
        overlayVisible_ = true;
    }
}

void OverlayManager::Hide()
{
    if (overlay_ && handle_) {
        overlay_->HideOverlay(handle_);
        overlayVisible_ = false;
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

void OverlayManager::SetTexture(ID3D11ShaderResourceView* srv)
{
    if (!overlay_ || !handle_ || !srv) return;

    vr::Texture_t texture;
    texture.handle = srv;
    texture.eType = vr::TextureType_DirectX;
    texture.eColorSpace = vr::ColorSpace_Auto;

    overlay_->SetOverlayTexture(handle_, &texture);
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
