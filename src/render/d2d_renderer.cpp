#include "render/d2d_renderer.h"
#include <dxgi1_2.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

namespace vrperf {

D2DRenderer::D2DRenderer() = default;

D2DRenderer::~D2DRenderer()
{
    Shutdown();
}

bool D2DRenderer::Initialize(ID3D11Device* d3dDevice)
{
    if (!d3dDevice) return false;

    // Create D2D factory
    D2D1_FACTORY_OPTIONS opts = {};
#ifdef _DEBUG
    opts.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

    HRESULT hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory1),
        &opts,
        reinterpret_cast<void**>(&factory_)
    );
    if (FAILED(hr)) return false;

    // Create DirectWrite factory
    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&dwriteFactory_)
    );
    if (FAILED(hr)) return false;

    // Create text formats
    auto createFormat = [&](float size, IDWriteTextFormat** fmt) -> bool {
        hr = dwriteFactory_->CreateTextFormat(
            L"Segoe UI", nullptr,
            DWRITE_FONT_WEIGHT_REGULAR,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            size, L"en-us", fmt
        );
        return SUCCEEDED(hr);
    };

    if (!createFormat(32.0f, &titleFormat_)) return false;
    if (!createFormat(28.0f, &valueFormat_)) return false;
    if (!createFormat(20.0f, &unitFormat_)) return false;
    if (!createFormat(18.0f, &smallFormat_)) return false;

    // Set text alignment
    titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    valueFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    unitFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

    // Create D2D device from D3D11 device
    IDXGIDevice* dxgiDevice = nullptr;
    hr = d3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
    if (FAILED(hr)) return false;

    hr = factory_->CreateDevice(dxgiDevice, &d2dDevice_);
    dxgiDevice->Release();
    if (FAILED(hr)) return false;

    // Create D2D device context
    hr = d2dDevice_->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
        &context_
    );
    if (FAILED(hr)) return false;

    // Set antialiasing mode for text
    context_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

    // Create brushes
    auto createBrush = [&](D2D1_COLOR_F color, ID2D1SolidColorBrush** brush) -> bool {
        return SUCCEEDED(context_->CreateSolidColorBrush(color, brush));
    };

    if (!createBrush(theme_.background, &bgBrush_)) return false;
    if (!createBrush(theme_.textPrimary, &textPrimaryBrush_)) return false;
    if (!createBrush(theme_.textSecondary, &textSecondaryBrush_)) return false;
    if (!createBrush(theme_.accentGood, &goodBrush_)) return false;
    if (!createBrush(theme_.accentWarning, &warningBrush_)) return false;
    if (!createBrush(theme_.accentCritical, &criticalBrush_)) return false;
    if (!createBrush(theme_.barBackground, &barBgBrush_)) return false;
    if (!createBrush(theme_.barFill, &barFillBrush_)) return false;
    if (!createBrush(theme_.panelBackground, &panelBgBrush_)) return false;
    if (!createBrush(theme_.divider, &dividerBrush_)) return false;

    return true;
}

void D2DRenderer::Shutdown()
{
    if (targetBitmap_) { targetBitmap_->Release(); targetBitmap_ = nullptr; }
    if (context_) { context_->Release(); context_ = nullptr; }
    if (d2dDevice_) { d2dDevice_->Release(); d2dDevice_ = nullptr; }
    if (factory_) { factory_->Release(); factory_ = nullptr; }

    if (bgBrush_) { bgBrush_->Release(); bgBrush_ = nullptr; }
    if (textPrimaryBrush_) { textPrimaryBrush_->Release(); textPrimaryBrush_ = nullptr; }
    if (textSecondaryBrush_) { textSecondaryBrush_->Release(); textSecondaryBrush_ = nullptr; }
    if (goodBrush_) { goodBrush_->Release(); goodBrush_ = nullptr; }
    if (warningBrush_) { warningBrush_->Release(); warningBrush_ = nullptr; }
    if (criticalBrush_) { criticalBrush_->Release(); criticalBrush_ = nullptr; }
    if (barBgBrush_) { barBgBrush_->Release(); barBgBrush_ = nullptr; }
    if (barFillBrush_) { barFillBrush_->Release(); barFillBrush_ = nullptr; }
    if (panelBgBrush_) { panelBgBrush_->Release(); panelBgBrush_ = nullptr; }
    if (dividerBrush_) { dividerBrush_->Release(); dividerBrush_ = nullptr; }

    if (titleFormat_) { titleFormat_->Release(); titleFormat_ = nullptr; }
    if (valueFormat_) { valueFormat_->Release(); valueFormat_ = nullptr; }
    if (unitFormat_) { unitFormat_->Release(); unitFormat_ = nullptr; }
    if (smallFormat_) { smallFormat_->Release(); smallFormat_ = nullptr; }
    if (dwriteFactory_) { dwriteFactory_->Release(); dwriteFactory_ = nullptr; }

    width_ = 0;
    height_ = 0;
}

void D2DRenderer::Resize(ID3D11Device* d3dDevice, uint32_t width, uint32_t height)
{
    (void)d3dDevice;

    if (width == width_ && height == height_) return;

    if (targetBitmap_) {
        targetBitmap_->Release();
        targetBitmap_ = nullptr;
    }

    width_ = width;
    height_ = height;
    // Bitmap will be recreated on next BeginDraw when texture is available
}

bool D2DRenderer::SetTargetTexture(ID3D11Texture2D* texture)
{
    if (!texture || !context_) {
        return false;
    }

    return CreateTargetBitmap(texture);
}

bool D2DRenderer::CreateTargetBitmap(ID3D11Texture2D* texture)
{
    if (targetBitmap_) {
        targetBitmap_->Release();
        targetBitmap_ = nullptr;
    }

    D3D11_TEXTURE2D_DESC textureDesc = {};
    texture->GetDesc(&textureDesc);

    IDXGISurface* surface = nullptr;
    HRESULT hr = texture->QueryInterface(
        __uuidof(IDXGISurface),
        reinterpret_cast<void**>(&surface)
    );
    if (FAILED(hr)) {
        return false;
    }

    D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET,
        D2D1::PixelFormat(
            DXGI_FORMAT_B8G8R8A8_UNORM,
            D2D1_ALPHA_MODE_PREMULTIPLIED
        )
    );

    hr = context_->CreateBitmapFromDxgiSurface(surface, &props, &targetBitmap_);
    surface->Release();

    if (FAILED(hr)) {
        targetBitmap_ = nullptr;
        return false;
    }

    width_ = textureDesc.Width;
    height_ = textureDesc.Height;
    context_->SetTarget(targetBitmap_);
    return true;
}

void D2DRenderer::BeginDraw()
{
    if (!context_) return;

    // If we have a target bitmap, set it as the render target
    if (targetBitmap_) {
        context_->SetTarget(targetBitmap_);
    }

    context_->BeginDraw();
    context_->Clear(theme_.background);
    isDrawing_ = true;
}

void D2DRenderer::EndDraw()
{
    if (!context_ || !isDrawing_) return;

    context_->EndDraw();
    isDrawing_ = false;
}

void D2DRenderer::DrawPanelBackground(float width, float height)
{
    // Draw semi-transparent panel background
    D2D1_RECT_F panelRect = D2D1::RectF(0, 0, width, height);
    context_->FillRectangle(panelRect, panelBgBrush_);

    // Draw border
    context_->DrawRectangle(panelRect, dividerBrush_, 2.0f);
}

void D2DRenderer::DrawText(const std::wstring& text, D2D1_RECT_F rect,
                           IDWriteTextFormat* format, ID2D1Brush* brush)
{
    if (!context_ || !format || !brush) return;
    context_->DrawText(text.c_str(), static_cast<UINT32>(text.length()),
                       format, rect, brush);
}

void D2DRenderer::DrawRect(D2D1_RECT_F rect, ID2D1Brush* brush, float strokeWidth)
{
    if (!context_ || !brush) return;
    if (strokeWidth > 0) {
        context_->DrawRectangle(rect, brush, strokeWidth);
    } else {
        context_->FillRectangle(rect, brush);
    }
}

void D2DRenderer::DrawBar(D2D1_RECT_F rect, float percentage,
                          ID2D1Brush* bgBrush, ID2D1Brush* fillBrush)
{
    if (!context_) return;

    // Background
    context_->FillRectangle(rect, bgBrush);

    // Fill based on percentage
    float width = rect.right - rect.left;
    float fillWidth = width * (std::min)(percentage / 100.0f, 1.0f);
    D2D1_RECT_F fillRect = D2D1::RectF(rect.left, rect.top,
                                        rect.left + fillWidth, rect.bottom);
    context_->FillRectangle(fillRect, fillBrush);
}

D2D1_COLOR_F D2DRenderer::GetValueColor(SensorCategory category, double value) const
{
    switch (category) {
    case SensorCategory::CpuTemp:
    case SensorCategory::GpuTemp:
        if (value > 85.0) return theme_.accentCritical;
        if (value > 70.0) return theme_.accentWarning;
        return theme_.accentGood;

    case SensorCategory::CpuLoad:
    case SensorCategory::GpuLoad:
        if (value > 90.0) return theme_.accentCritical;
        if (value > 70.0) return theme_.accentWarning;
        return theme_.accentGood;

    default:
        return theme_.textPrimary;
    }
}

std::wstring D2DRenderer::FormatReading(const SensorReading& reading) const
{
    std::wstringstream ss;

    switch (reading.category) {
    case SensorCategory::CpuTemp:
    case SensorCategory::GpuTemp:
        ss << std::fixed << std::setprecision(0) << reading.value << L"°C";
        break;

    case SensorCategory::CpuLoad:
    case SensorCategory::GpuLoad:
    case SensorCategory::RamUsage:
        ss << std::fixed << std::setprecision(0) << reading.value << L"%";
        break;

    case SensorCategory::CpuClock:
    case SensorCategory::GpuClock:
        ss << std::fixed << std::setprecision(0) << reading.value << L" MHz";
        break;

    case SensorCategory::GpuMemory:
        // Convert MB to GB if large enough
        if (reading.value > 1024.0) {
            ss << std::fixed << std::setprecision(1) << (reading.value / 1024.0) << L" GB";
        } else {
            ss << std::fixed << std::setprecision(0) << reading.value << L" MB";
        }
        break;

    case SensorCategory::GpuFan:
    case SensorCategory::Fan:
        ss << std::fixed << std::setprecision(0) << reading.value << L" RPM";
        break;

    case SensorCategory::Power:
        ss << std::fixed << std::setprecision(1) << reading.value << L" W";
        break;

    case SensorCategory::Voltage:
        ss << std::fixed << std::setprecision(2) << reading.value << L" V";
        break;

    default:
        ss << std::fixed << std::setprecision(1) << reading.value;
        if (!reading.unit.empty()) {
            ss << L" " << std::wstring(reading.unit.begin(), reading.unit.end());
        }
        break;
    }

    return ss.str();
}

void D2DRenderer::DrawSensorPanel(const std::vector<SensorReading>& readings,
                                  const Config& config,
                                  double vrFrameTimeMs,
                                  uint32_t vrDroppedFrames)
{
    if (!context_ || !isDrawing_) return;

    float w = static_cast<float>(width_);
    float h = static_cast<float>(height_);

    // Draw panel background
    DrawPanelBackground(w, h);

    // Layout constants
    const float padding = 20.0f;
    const float rowHeight = 40.0f;
    const float barHeight = 12.0f;
    const float labelWidth = 150.0f;
    const float valueWidth = 120.0f;

    float y = padding;

    // Title
    D2D1_RECT_F titleRect = D2D1::RectF(padding, y, w - padding, y + 36.0f);
    DrawText(L"VR Performance Monitor", titleRect, titleFormat_, textPrimaryBrush_);
    y += 44.0f;

    // Divider
    D2D1_RECT_F dividerRect = D2D1::RectF(padding, y, w - padding, y + 1.0f);
    DrawRect(dividerRect, dividerBrush_);
    y += 8.0f;

    if (vrFrameTimeMs > 0.0) {
        std::wstringstream vrLine;
        vrLine << L"VR Frame: " << std::fixed << std::setprecision(1)
               << vrFrameTimeMs << L" ms  Drops: " << vrDroppedFrames;
        D2D1_RECT_F vrRect = D2D1::RectF(padding, y, w - padding, y + 28.0f);
        DrawText(vrLine.str(), vrRect, smallFormat_, textSecondaryBrush_);
        y += 32.0f;
    }

    // Filter and display enabled metrics
    for (const auto& metric : config.metrics) {
        if (!metric.enabled) continue;

        // Find matching readings for this metric category
        for (const auto& reading : readings) {
            if (reading.category != SensorCategory::Unknown) {
                // Simple category matching by string
                std::string catStr;
                switch (reading.category) {
                case SensorCategory::CpuLoad: catStr = "cpu_load"; break;
                case SensorCategory::CpuTemp: catStr = "cpu_temp"; break;
                case SensorCategory::GpuLoad: catStr = "gpu_load"; break;
                case SensorCategory::GpuTemp: catStr = "gpu_temp"; break;
                case SensorCategory::GpuClock: catStr = "gpu_clock"; break;
                case SensorCategory::GpuMemory: catStr = "gpu_memory"; break;
                case SensorCategory::RamUsage: catStr = "ram_usage"; break;
                case SensorCategory::GpuFan: catStr = "gpu_fan"; break;
                default: continue;
                }

                if (catStr != metric.category) continue;

                // Draw label
                std::wstring label(metric.label.begin(), metric.label.end());
                D2D1_RECT_F labelRect = D2D1::RectF(padding, y, padding + labelWidth, y + rowHeight);
                DrawText(label, labelRect, valueFormat_, textSecondaryBrush_);

                // Draw value
                std::wstring value = FormatReading(reading);
                D2D1_COLOR_F valueColor = GetValueColor(reading.category, reading.value);

                ID2D1SolidColorBrush* valueBrush = nullptr;
                context_->CreateSolidColorBrush(valueColor, &valueBrush);

                D2D1_RECT_F valueRect = D2D1::RectF(
                    w - padding - valueWidth, y,
                    w - padding, y + rowHeight
                );
                DrawText(value, valueRect, valueFormat_, valueBrush);

                if (valueBrush) valueBrush->Release();

                // Draw bar for load/temp categories
                if (reading.category == SensorCategory::CpuLoad ||
                    reading.category == SensorCategory::GpuLoad ||
                    reading.category == SensorCategory::CpuTemp ||
                    reading.category == SensorCategory::GpuTemp) {

                    float barY = y + rowHeight - barHeight - 4.0f;
                    D2D1_RECT_F barRect = D2D1::RectF(
                        padding + labelWidth + 10.0f, barY,
                        w - padding - valueWidth - 10.0f, barY + barHeight
                    );

                    float percentage = static_cast<float>(reading.value);
                    if (reading.category == SensorCategory::CpuTemp ||
                        reading.category == SensorCategory::GpuTemp) {
                        // Scale temp: 0-100°C → 0-100%
                        percentage = (std::min)(percentage, 100.0f);
                    }

                    DrawBar(barRect, percentage, barBgBrush_, barFillBrush_);
                }

                y += rowHeight;
                break; // Only show first matching reading per category
            }
        }
    }
}

} // namespace vrperf
