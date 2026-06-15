#pragma once

#include <openvr.h>
#include <string>
#include <d3d11.h>

namespace vrperf {

class OverlayManager {
public:
    OverlayManager();
    ~OverlayManager();

    // Initialize OpenVR as an overlay application
    bool Initialize();

    // Shutdown OpenVR
    void Shutdown();

    // Create the main overlay
    bool CreateOverlay(const std::string& key, const std::string& name);

    void SetWidthMeters(float widthMeters);
    void SetAlpha(float alpha);
    void SetInputNone();

    // Show/hide overlay
    void Show();
    void Hide();
    void ToggleVisibility();

    // Submit a D3D11 texture as overlay content
    void SetTexture(ID3D11ShaderResourceView* srv);

    // Poll OpenVR events (call each frame)
    void PollEvents();

    // Properties
    bool IsInitialized() const { return initialized_; }
    bool IsOverlayVisible() const { return overlayVisible_; }
    vr::IVROverlay* GetOverlay() const { return overlay_; }
    vr::VROverlayHandle_t GetHandle() const { return handle_; }

private:
    bool initialized_ = false;
    bool overlayVisible_ = false;

    vr::IVRSystem* system_ = nullptr;
    vr::IVROverlay* overlay_ = nullptr;
    vr::VROverlayHandle_t handle_ = 0;
};

} // namespace vrperf
