#pragma once

#include <d3d11.h>
#include <cstdint>

namespace vrperf {

class D3D11Renderer {
public:
    D3D11Renderer();
    ~D3D11Renderer();

    bool Initialize(uint32_t width = 1024, uint32_t height = 512);
    void Shutdown();

    void Resize(uint32_t width, uint32_t height);

    ID3D11Device* GetDevice() const { return device_; }
    ID3D11DeviceContext* GetContext() const { return context_; }
    ID3D11Texture2D* GetTexture() const { return texture_; }
    ID3D11ShaderResourceView* GetSRV() const { return srv_; }

    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }

private:
    bool CreateTexture(uint32_t width, uint32_t height);

    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    ID3D11Texture2D* texture_ = nullptr;
    ID3D11ShaderResourceView* srv_ = nullptr;

    uint32_t width_ = 0;
    uint32_t height_ = 0;
};

} // namespace vrperf
