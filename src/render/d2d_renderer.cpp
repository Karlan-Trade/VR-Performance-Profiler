#include "render/d2d_renderer.h"
#include <dxgi1_2.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cwctype>
#include <windows.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

namespace vrperf {

namespace {

std::wstring NarrowToWide(const std::string& text)
{
    if (text.empty()) {
        return {};
    }

    int length = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0);
    UINT codePage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    if (length <= 0) {
        codePage = CP_ACP;
        flags = 0;
        length = MultiByteToWideChar(
            codePage,
            flags,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0);
    }
    if (length <= 0) {
        return std::wstring(text.begin(), text.end());
    }

    std::wstring wide(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(
        codePage,
        flags,
        text.data(),
        static_cast<int>(text.size()),
        wide.data(),
        length);
    return wide;
}

std::wstring Ellipsize(std::wstring text, size_t maxChars)
{
    if (text.size() <= maxChars || maxChars < 2) {
        return text;
    }

    text.resize(maxChars - 1);
    text.push_back(L'\u2026');
    return text;
}

std::wstring Trim(std::wstring text)
{
    const auto isSpace = [](wchar_t ch) {
        return std::iswspace(ch) != 0;
    };
    text.erase(text.begin(), std::find_if_not(text.begin(), text.end(), isSpace));
    text.erase(std::find_if_not(text.rbegin(), text.rend(), isSpace).base(), text.end());
    return text;
}

bool StartsWithCaseInsensitive(const std::wstring& text, const std::wstring& prefix)
{
    if (prefix.empty() || text.size() < prefix.size()) {
        return false;
    }

    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::towlower(text[i]) != std::towlower(prefix[i])) {
            return false;
        }
    }
    return true;
}

std::wstring FirstWord(const std::wstring& text)
{
    const auto pos = text.find_first_of(L" \t\r\n");
    return pos == std::wstring::npos ? text : text.substr(0, pos);
}

std::wstring CollapseRepeatedPrefix(std::wstring text)
{
    const auto word = FirstWord(text);
    if (word.empty()) {
        return text;
    }

    const auto repeated = word + L" " + word;
    if (StartsWithCaseInsensitive(text, repeated)) {
        text.erase(0, repeated.size());
        text = word + L" " + Trim(std::move(text));
    }

    return text;
}

} // namespace

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
            L"Microsoft YaHei UI", nullptr,
            DWRITE_FONT_WEIGHT_REGULAR,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            size, L"zh-cn", fmt
        );
        return SUCCEEDED(hr);
    };

    if (!createFormat(28.0f, &titleFormat_)) return false;
    if (!createFormat(24.0f, &labelFormat_)) return false;
    if (!createFormat(24.0f, &valueFormat_)) return false;
    if (!createFormat(18.0f, &compactValueFormat_)) return false;
    if (!createFormat(20.0f, &unitFormat_)) return false;
    if (!createFormat(18.0f, &smallFormat_)) return false;

    // Set text alignment
    titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    labelFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    valueFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    compactValueFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    unitFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    titleFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    labelFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    valueFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    compactValueFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    smallFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

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
    if (labelFormat_) { labelFormat_->Release(); labelFormat_ = nullptr; }
    if (valueFormat_) { valueFormat_->Release(); valueFormat_ = nullptr; }
    if (compactValueFormat_) { compactValueFormat_->Release(); compactValueFormat_ = nullptr; }
    if (unitFormat_) { unitFormat_->Release(); unitFormat_ = nullptr; }
    if (smallFormat_) { smallFormat_->Release(); smallFormat_ = nullptr; }
    if (dwriteFactory_) { dwriteFactory_->Release(); dwriteFactory_ = nullptr; }

    width_ = 0;
    height_ = 0;
}

void D2DRenderer::SetTheme(const ColorTheme& theme)
{
    theme_ = theme;

    if (bgBrush_) bgBrush_->SetColor(theme_.background);
    if (textPrimaryBrush_) textPrimaryBrush_->SetColor(theme_.textPrimary);
    if (textSecondaryBrush_) textSecondaryBrush_->SetColor(theme_.textSecondary);
    if (goodBrush_) goodBrush_->SetColor(theme_.accentGood);
    if (warningBrush_) warningBrush_->SetColor(theme_.accentWarning);
    if (criticalBrush_) criticalBrush_->SetColor(theme_.accentCritical);
    if (barBgBrush_) barBgBrush_->SetColor(theme_.barBackground);
    if (barFillBrush_) barFillBrush_->SetColor(theme_.barFill);
    if (panelBgBrush_) panelBgBrush_->SetColor(theme_.panelBackground);
    if (dividerBrush_) dividerBrush_->SetColor(theme_.divider);
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
    context_->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
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

float D2DRenderer::MeasureTextWidth(const std::wstring& text, IDWriteTextFormat* format) const
{
    if (!dwriteFactory_ || !format || text.empty()) {
        return 0.0f;
    }

    IDWriteTextLayout* layout = nullptr;
    HRESULT hr = dwriteFactory_->CreateTextLayout(
        text.c_str(),
        static_cast<UINT32>(text.size()),
        format,
        4096.0f,
        256.0f,
        &layout);
    if (FAILED(hr) || !layout) {
        return 0.0f;
    }

    DWRITE_TEXT_METRICS metrics = {};
    hr = layout->GetMetrics(&metrics);
    layout->Release();
    return SUCCEEDED(hr) ? metrics.widthIncludingTrailingWhitespace : 0.0f;
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

std::wstring FormatOverlayReadingLabel(const SensorReading& reading)
{
    auto label = Trim(NarrowToWide(reading.label));
    const auto device = Trim(NarrowToWide(reading.device));
    if (!device.empty() && StartsWithCaseInsensitive(label, device)) {
        auto suffix = label.substr(device.size());
        if (suffix.empty() || std::iswspace(suffix.front())) {
            suffix = Trim(std::move(suffix));
            if (!suffix.empty()) {
                label = suffix;
            }
        }
    }
    label = CollapseRepeatedPrefix(std::move(label));
    return label.empty() ? NarrowToWide(SensorCategoryKey(reading.category)) : label;
}

std::wstring FormatOverlayReadingValue(const SensorReading& reading)
{
    if (!std::isfinite(reading.value)) {
        return L"--";
    }

    std::wstringstream ss;

    switch (reading.category) {
    case SensorCategory::CpuTemp:
    case SensorCategory::GpuTemp:
        ss << std::fixed << std::setprecision(0) << reading.value << L"\u00B0C";
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
        if (reading.unit == "%") {
            ss << std::fixed << std::setprecision(0) << reading.value << L"%";
        } else if (reading.unit.empty() || reading.unit == "MB") {
            if (reading.value > 1024.0) {
                ss << std::fixed << std::setprecision(1) << (reading.value / 1024.0) << L" GB";
            } else {
                ss << std::fixed << std::setprecision(0) << reading.value << L" MB";
            }
        } else {
            ss << std::fixed << std::setprecision(1) << reading.value
               << L" " << NarrowToWide(reading.unit);
        }
        break;

    case SensorCategory::GpuFan:
    case SensorCategory::Fan:
        if (reading.unit == "%") {
            ss << std::fixed << std::setprecision(0) << reading.value << L"%";
        } else {
            ss << std::fixed << std::setprecision(0) << reading.value << L" RPM";
        }
        break;

    case SensorCategory::Power:
        ss << std::fixed << std::setprecision(1) << reading.value << L" W";
        break;

    case SensorCategory::Voltage:
        ss << std::fixed << std::setprecision(2) << reading.value << L" V";
        break;

    case SensorCategory::VrFps:
        ss << std::fixed << std::setprecision(1) << reading.value << L" FPS";
        break;

    case SensorCategory::VrFrameTime:
    case SensorCategory::VrGpuFrameTime:
        ss << std::fixed << std::setprecision(1) << reading.value << L" ms";
        break;

    case SensorCategory::VrRefreshRate:
        ss << std::fixed << std::setprecision(0) << reading.value << L" Hz";
        break;

    case SensorCategory::VrDroppedFrames:
        ss << std::fixed << std::setprecision(0) << reading.value;
        break;

    default:
        ss << std::fixed << std::setprecision(1) << reading.value;
        if (!reading.unit.empty()) {
            ss << L" " << NarrowToWide(reading.unit);
        }
        break;
    }

    return ss.str();
}

std::wstring D2DRenderer::FormatReading(const SensorReading& reading) const
{
    return FormatOverlayReadingValue(reading);
}

void D2DRenderer::DrawSensorPanel(const std::vector<SensorReading>& readings,
                                  const Config& config)
{
    if (!context_ || !isDrawing_) return;
    const bool english = config.general.language == "en";

    const float textureWidth = static_cast<float>(width_);
    const float textureHeight = static_cast<float>(height_);

    // Layout constants
    const float padding = 20.0f;
    const float barHeight = 8.0f;
    const float minValueWidth = 80.0f;
    const float gap = 12.0f;

    const auto title = english ? L"VR Performance Monitor" : L"VR \u6027\u80FD\u76D1\u63A7";
    const auto visibleRows = (std::max)(size_t{1}, readings.size());
    const float titleBlockHeight = 34.0f;
    const float dividerBlockHeight = 11.0f;
    const float panelHeight = textureHeight;
    const float availableRowsHeight = (std::max)(
        40.0f,
        panelHeight - padding * 2.0f - titleBlockHeight - dividerBlockHeight - 6.0f);
    const float rowHeight = (std::max)(
        24.0f,
        (std::min)(40.0f, availableRowsHeight / static_cast<float>(visibleRows)));
    IDWriteTextFormat* labelRowFormat = rowHeight < 32.0f
        ? smallFormat_
        : labelFormat_;
    IDWriteTextFormat* valueRowFormat = rowHeight < 32.0f
        ? compactValueFormat_
        : valueFormat_;

    std::vector<std::wstring> labels;
    std::vector<std::wstring> values;
    labels.reserve(readings.size());
    values.reserve(readings.size());

    float maxLabelWidth = 0.0f;
    float maxValueWidth = minValueWidth;
    for (const auto& reading : readings) {
        labels.push_back(FormatOverlayReadingLabel(reading));
        values.push_back(FormatReading(reading));
        maxLabelWidth = (std::max)(maxLabelWidth, MeasureTextWidth(labels.back(), labelRowFormat));
        maxValueWidth = (std::max)(maxValueWidth, MeasureTextWidth(values.back(), valueRowFormat));
    }

    const float w = textureWidth;
    const float valueWidth = (std::min)(
        (std::max)(minValueWidth, maxValueWidth + 8.0f),
        w - padding * 2.0f - gap - 80.0f);
    const float labelWidth = (std::max)(
        80.0f,
        w - padding * 2.0f - valueWidth - gap);
    const size_t labelChars = static_cast<size_t>(
        (std::max)(8.0f, labelWidth / (rowHeight < 32.0f ? 10.0f : 13.0f)));

    DrawPanelBackground(w, panelHeight);

    float y = padding;

    D2D1_RECT_F titleRect = D2D1::RectF(padding, y, w - padding, y + 30.0f);
    DrawText(
        title,
        titleRect,
        titleFormat_,
        textPrimaryBrush_);
    y += 34.0f;

    // Divider
    D2D1_RECT_F dividerRect = D2D1::RectF(padding, y, w - padding, y + 1.0f);
    DrawRect(dividerRect, dividerBrush_);
    y += 10.0f;

    // Readings are already filtered and ordered by the user's exact selections.
    for (size_t index = 0; index < readings.size(); ++index) {
        const auto& reading = readings[index];
        if (y + rowHeight > panelHeight - padding * 0.5f) {
            break;
        }

        std::wstring label = Ellipsize(labels[index], labelChars);
        const bool hasBar = reading.category == SensorCategory::CpuLoad ||
            reading.category == SensorCategory::GpuLoad ||
            reading.category == SensorCategory::CpuTemp ||
            reading.category == SensorCategory::GpuTemp;
        const bool drawBar = hasBar && rowHeight >= 34.0f;
        const float textBottom = drawBar
            ? y + rowHeight - barHeight - 5.0f
            : y + rowHeight;

        D2D1_RECT_F labelRect = D2D1::RectF(
            padding,
            y,
            padding + labelWidth,
            textBottom);
        DrawText(label, labelRect, labelRowFormat, textSecondaryBrush_);

        const std::wstring& value = values[index];
        D2D1_COLOR_F valueColor = GetValueColor(reading.category, reading.value);

        ID2D1SolidColorBrush* valueBrush = nullptr;
        context_->CreateSolidColorBrush(valueColor, &valueBrush);

        D2D1_RECT_F valueRect = D2D1::RectF(
            w - padding - valueWidth,
            y,
            w - padding,
            textBottom);
        DrawText(value, valueRect, valueRowFormat, valueBrush);

        if (valueBrush) {
            valueBrush->Release();
        }

        if (drawBar) {
            float barY = y + rowHeight - barHeight - 2.0f;
            D2D1_RECT_F barRect = D2D1::RectF(
                padding,
                barY,
                w - padding,
                barY + barHeight);

            float percentage = static_cast<float>(reading.value);
            if (reading.category == SensorCategory::CpuTemp ||
                reading.category == SensorCategory::GpuTemp) {
                percentage = (std::min)(percentage, 100.0f);
            }

            DrawBar(barRect, percentage, barBgBrush_, barFillBrush_);
        }

        y += rowHeight;
    }
}

} // namespace vrperf
