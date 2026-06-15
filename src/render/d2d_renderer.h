#pragma once

#include "hwinfo/sensor_data.h"
#include "core/config.h"
#include "render/color_theme.h"

#include <d2d1.h>
#include <d2d1_1.h>
#include <d3d11.h>
#include <dwrite.h>
#include <dxgi.h>
#include <cstdint>
#include <vector>

namespace vrperf {

class D2DRenderer {
public:
    D2DRenderer();
    ~D2DRenderer();

    // Initialize with a D3D11 device
    bool Initialize(ID3D11Device* d3dDevice);
    void Shutdown();

    // Resize render target
    void Resize(ID3D11Device* d3dDevice, uint32_t width, uint32_t height);
    bool SetTargetTexture(ID3D11Texture2D* texture);

    // Drawing
    void BeginDraw();
    void EndDraw();

    // Render sensor panel
    void DrawSensorPanel(const std::vector<SensorReading>& readings,
                         const Config& config);

    // Set theme
    void SetTheme(const ColorTheme& theme);

private:
    bool CreateTargetBitmap(ID3D11Texture2D* texture);

    // Drawing helpers
    void DrawPanelBackground(float width, float height);
    void DrawText(const std::wstring& text, D2D1_RECT_F rect,
                  IDWriteTextFormat* format, ID2D1Brush* brush);
    void DrawRect(D2D1_RECT_F rect, ID2D1Brush* brush, float strokeWidth = 0);
    void DrawBar(D2D1_RECT_F rect, float percentage, ID2D1Brush* bgBrush, ID2D1Brush* fillBrush);

    // Get value color based on thresholds
    D2D1_COLOR_F GetValueColor(SensorCategory category, double value) const;

    // Convert sensor reading to display string
    std::wstring FormatReading(const SensorReading& reading) const;

    // D2D objects
    ID2D1Factory1* factory_ = nullptr;
    ID2D1Device* d2dDevice_ = nullptr;
    ID2D1DeviceContext* context_ = nullptr;
    ID2D1Bitmap1* targetBitmap_ = nullptr;

    // Brushes
    ID2D1SolidColorBrush* bgBrush_ = nullptr;
    ID2D1SolidColorBrush* textPrimaryBrush_ = nullptr;
    ID2D1SolidColorBrush* textSecondaryBrush_ = nullptr;
    ID2D1SolidColorBrush* goodBrush_ = nullptr;
    ID2D1SolidColorBrush* warningBrush_ = nullptr;
    ID2D1SolidColorBrush* criticalBrush_ = nullptr;
    ID2D1SolidColorBrush* barBgBrush_ = nullptr;
    ID2D1SolidColorBrush* barFillBrush_ = nullptr;
    ID2D1SolidColorBrush* panelBgBrush_ = nullptr;
    ID2D1SolidColorBrush* dividerBrush_ = nullptr;

    // DirectWrite
    IDWriteFactory* dwriteFactory_ = nullptr;
    IDWriteTextFormat* titleFormat_ = nullptr;
    IDWriteTextFormat* labelFormat_ = nullptr;
    IDWriteTextFormat* valueFormat_ = nullptr;
    IDWriteTextFormat* compactValueFormat_ = nullptr;
    IDWriteTextFormat* unitFormat_ = nullptr;
    IDWriteTextFormat* smallFormat_ = nullptr;

    ColorTheme theme_ = DarkTheme();
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    bool isDrawing_ = false;
};

} // namespace vrperf
