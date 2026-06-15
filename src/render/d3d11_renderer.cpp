#include "render/d3d11_renderer.h"
#include <iostream>

namespace vrperf {

D3D11Renderer::D3D11Renderer() = default;

D3D11Renderer::~D3D11Renderer()
{
    Shutdown();
}

bool D3D11Renderer::Initialize(uint32_t width, uint32_t height)
{
    if (device_) return true; // Already initialized

    UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    // Create D3D11 device
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(
        nullptr,                    // Default adapter
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,                    // No software module
        createFlags,
        nullptr,                    // Default feature levels
        0,
        D3D11_SDK_VERSION,
        &device_,
        &featureLevel,
        &context_
    );

#ifdef _DEBUG
    if (FAILED(hr)) {
        createFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            createFlags,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &device_,
            &featureLevel,
            &context_
        );
    }
#endif

    if (FAILED(hr)) {
        std::cerr << "D3D11CreateDevice failed: 0x" << std::hex << hr << std::endl;
        return false;
    }

    if (!CreateTexture(width, height)) {
        Shutdown();
        return false;
    }

    return true;
}

void D3D11Renderer::Shutdown()
{
    if (srv_) { srv_->Release(); srv_ = nullptr; }
    if (texture_) { texture_->Release(); texture_ = nullptr; }
    if (context_) { context_->Release(); context_ = nullptr; }
    if (device_) { device_->Release(); device_ = nullptr; }
    width_ = 0;
    height_ = 0;
}

bool D3D11Renderer::CreateTexture(uint32_t width, uint32_t height)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // D2D compatible
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    HRESULT hr = device_->CreateTexture2D(&desc, nullptr, &texture_);
    if (FAILED(hr)) {
        std::cerr << "CreateTexture2D failed: 0x" << std::hex << hr << std::endl;
        return false;
    }

    // Create shader resource view for OpenVR
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;

    hr = device_->CreateShaderResourceView(texture_, &srvDesc, &srv_);
    if (FAILED(hr)) {
        std::cerr << "CreateShaderResourceView failed: 0x" << std::hex << hr << std::endl;
        return false;
    }

    width_ = width;
    height_ = height;
    return true;
}

void D3D11Renderer::Resize(uint32_t width, uint32_t height)
{
    if (width == width_ && height == height_) return;

    if (srv_) { srv_->Release(); srv_ = nullptr; }
    if (texture_) { texture_->Release(); texture_ = nullptr; }

    CreateTexture(width, height);
}

} // namespace vrperf
