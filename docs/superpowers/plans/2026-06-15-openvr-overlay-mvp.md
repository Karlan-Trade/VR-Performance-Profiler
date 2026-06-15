# OpenVR Overlay MVP 实现计划

**目标：** 让 VR Performance Profiler 以类似 VRCX 的方式，在 SteamVR 中通过 OpenVR overlay 显示实时硬件和 VR 性能数据。

**架构：** 项目保持独立进程运行，不注入 VR 游戏、不 Hook 游戏渲染链。数据层读取 HWiNFO/系统/SteamVR 指标，显示层使用 Direct2D 绘制 UI 到 Direct3D 11 纹理，再通过 `IVROverlay::SetOverlayTexture` 交给 SteamVR compositor 显示。

**技术栈：** C++17、CMake、OpenVR、Direct3D 11、Direct2D、DirectWrite、HWiNFO Shared Memory、Windows API。

---

## 文件结构

- 修改：`src/render/d2d_renderer.h`
  - 负责公开 D2D render target 绑定入口，让 D2D 能画到 `D3D11Renderer` 创建的 texture。
- 修改：`src/render/d2d_renderer.cpp`
  - 负责把 `ID3D11Texture2D` 转成 `IDXGISurface`，再创建 `ID2D1Bitmap1` 作为 D2D target。
- 修改：`src/render/d3d11_renderer.cpp`
  - 负责确保 D3D11 device 支持 BGRA 和 Direct2D interop。
- 修改：`src/overlay/overlay_manager.h`
  - 负责提供 overlay 配置接口，如 alpha、宽度、输入方式。
- 修改：`src/overlay/overlay_manager.cpp`
  - 负责创建 overlay、设置纹理属性、提交 texture，并保持只和 OpenVR 交互。
- 修改：`src/overlay/overlay_positioner.h`
  - 负责暴露可测试的矩阵生成接口。
- 修改：`src/overlay/overlay_positioner.cpp`
  - 负责修正控制器枚举范围，按左右手选择 wrist overlay 目标。
- 修改：`src/core/app.cpp`
  - 负责初始化顺序、创建 overlay、绑定 D2D target、刷新数据、显示/隐藏。
- 创建：`src/data/perf_snapshot.h`
  - 负责统一 UI 所需的性能数据快照。
- 创建：`src/data/windows_fallback_provider.h`
- 创建：`src/data/windows_fallback_provider.cpp`
  - 负责在 HWiNFO 不可用时提供 CPU/RAM 基础指标。
- 创建：`src/vr/openvr_frame_timing.h`
- 创建：`src/vr/openvr_frame_timing.cpp`
  - 负责读取 OpenVR frame timing 指标。
- 修改：`CMakeLists.txt`
  - 负责加入新增源文件。
- 创建：`tests/test_overlay_positioner.cpp`
  - 负责验证 HUD/wrist transform 的基础矩阵行为。
- 创建：`tests/test_windows_fallback_provider.cpp`
  - 负责验证 Windows fallback provider 返回稳定、范围合理的指标。
- 修改：`tests/CMakeLists.txt`
  - 负责注册新增测试。
- 修改：`README.md`
  - 负责记录运行前提、安全边界和反作弊风险说明。
- 修改：`docs/research-notes.md`
  - 负责补充最终架构决策和验收结论。

---

## 里程碑 1：让 D2D 真正画到 D3D11 texture

### 任务 1：为 D2DRenderer 增加 texture target 绑定

**文件：**
- 修改：`src/render/d2d_renderer.h`
- 修改：`src/render/d2d_renderer.cpp`
- 修改：`src/core/app.cpp`

- [ ] **步骤 1：修改头文件接口**

在 `src/render/d2d_renderer.h` 的 public 区域加入：

```cpp
bool SetTargetTexture(ID3D11Texture2D* texture);
```

把 private 区域的 `CreateResources(ID3D11Device* d3dDevice, uint32_t width, uint32_t height)` 替换为：

```cpp
bool CreateTargetBitmap(ID3D11Texture2D* texture);
```

- [ ] **步骤 2：实现 D3D11 texture 到 D2D bitmap 的绑定**

在 `src/render/d2d_renderer.cpp` 中实现：

```cpp
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
    HRESULT hr = texture->QueryInterface(__uuidof(IDXGISurface),
        reinterpret_cast<void**>(&surface));
    if (FAILED(hr)) {
        return false;
    }

    D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                          D2D1_ALPHA_MODE_PREMULTIPLIED)
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
```

- [ ] **步骤 3：让 App 初始化后绑定 target**

在 `src/core/app.cpp` 中，`d2dRenderer_.Initialize(...)` 成功后加入：

```cpp
if (!d2dRenderer_.SetTargetTexture(d3d11Renderer_.GetTexture())) {
    return false;
}
```

- [ ] **步骤 4：运行构建验证**

运行：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

预期：`vr_perf_core` 和 `vr_perf_profiler` 编译通过。

---

### 任务 2：修正 D3D11 device flag，保证 Direct2D interop

**文件：**
- 修改：`src/render/d3d11_renderer.cpp`

- [ ] **步骤 1：修改 device 创建 flags**

在 `D3D11Renderer::Initialize` 中，把 `D3D11CreateDevice` 的 flags 参数从 `0` 改为：

```cpp
UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
```

并传入：

```cpp
createFlags,
```

- [ ] **步骤 2：运行构建验证**

运行：

```powershell
cmake --build build --config Debug
```

预期：Debug 构建通过。若本机没有 D3D debug layer，移除 `_DEBUG` 下的 `D3D11_CREATE_DEVICE_DEBUG`，保留 `D3D11_CREATE_DEVICE_BGRA_SUPPORT`。

---

## 里程碑 2：让 OpenVR overlay 完整创建、定位、显示

### 任务 3：在 App 初始化中创建 overlay

**文件：**
- 修改：`src/core/app.cpp`
- 修改：`src/overlay/overlay_manager.h`
- 修改：`src/overlay/overlay_manager.cpp`

- [ ] **步骤 1：给 OverlayManager 增加配置方法**

在 `src/overlay/overlay_manager.h` public 区域加入：

```cpp
void SetWidthMeters(float widthMeters);
void SetAlpha(float alpha);
void SetInputNone();
```

在 `src/overlay/overlay_manager.cpp` 中实现：

```cpp
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
```

- [ ] **步骤 2：在 App 中创建和配置 overlay**

在 `App::Initialize()` 中，`overlayManager_.Initialize()` 成功后加入：

```cpp
if (!overlayManager_.CreateOverlay(
        "vrperf.profiler.overlay",
        "VR Performance Profiler")) {
    return false;
}

overlayManager_.SetWidthMeters(config_.overlay.widthMeters);
overlayManager_.SetAlpha(config_.overlay.alpha);
overlayManager_.SetInputNone();
overlayPositioner_.SetHudPosition(
    config_.hud.yawDegrees,
    config_.hud.pitchDegrees,
    config_.hud.distanceMeters);
overlayPositioner_.SetMode(config_.overlay.mode == "wrist"
    ? OverlayMode::Wrist
    : OverlayMode::HUD);
overlayPositioner_.SetWristHand(config_.wrist.hand != "right");
overlayPositioner_.ApplyTransform(&overlayManager_);

if (config_.overlay.visibleOnStart) {
    overlayManager_.Show();
}
```

- [ ] **步骤 3：运行构建验证**

运行：

```powershell
cmake --build build --config Debug
```

预期：构建通过。

---

### 任务 4：修正 wrist controller 选择逻辑

**文件：**
- 修改：`src/overlay/overlay_positioner.h`
- 修改：`src/overlay/overlay_positioner.cpp`
- 创建：`tests/test_overlay_positioner.cpp`
- 修改：`tests/CMakeLists.txt`

- [ ] **步骤 1：暴露 transform 生成函数用于测试**

在 `src/overlay/overlay_positioner.h` 中，把以下声明移到 public 区域：

```cpp
vr::HmdMatrix34_t MakeHudTransform() const;
vr::HmdMatrix34_t MakeWristTransform() const;
```

- [ ] **步骤 2：修正控制器枚举范围**

在 `OverlayPositioner::ApplyTransform` wrist 分支中，把循环：

```cpp
for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unTrackedDeviceIndex_Hmd; ++i)
```

替换为：

```cpp
for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
```

- [ ] **步骤 3：加入基础 transform 测试**

创建 `tests/test_overlay_positioner.cpp`：

```cpp
#include "overlay/overlay_positioner.h"

#include <cassert>
#include <cmath>
#include <iostream>

static bool Near(float a, float b)
{
    return std::fabs(a - b) < 0.0001f;
}

int main()
{
    vrperf::OverlayPositioner positioner;

    positioner.SetHudPosition(0.0f, 0.0f, 1.25f);
    auto hud = positioner.MakeHudTransform();
    assert(Near(hud.m[0][0], 1.0f));
    assert(Near(hud.m[1][1], 1.0f));
    assert(Near(hud.m[2][2], 1.0f));
    assert(Near(hud.m[2][3], -1.25f));

    positioner.SetWristHand(true);
    auto left = positioner.MakeWristTransform();
    assert(left.m[0][3] > 0.0f);

    positioner.SetWristHand(false);
    auto right = positioner.MakeWristTransform();
    assert(right.m[0][3] <= 0.0f);

    std::cout << "[PASS] OverlayPositioner transform tests passed\n";
    return 0;
}
```

- [ ] **步骤 4：注册测试**

在 `tests/CMakeLists.txt` 追加：

```cmake
add_executable(test_overlay_positioner
    test_overlay_positioner.cpp
)

target_link_libraries(test_overlay_positioner PRIVATE
    vr_perf_core
)

add_test(NAME OverlayPositioner COMMAND test_overlay_positioner)
```

- [ ] **步骤 5：运行测试**

运行：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

预期：`OverlayPositioner`、`Config`、`HwInfoReader` 测试通过。

---

## 里程碑 3：数据层从“只有 HWiNFO”升级为“主源 + 兜底”

### 任务 5：定义 UI 性能快照

**文件：**
- 创建：`src/data/perf_snapshot.h`

- [ ] **步骤 1：创建统一数据结构**

创建 `src/data/perf_snapshot.h`：

```cpp
#pragma once

#include "hwinfo/sensor_data.h"

#include <string>
#include <vector>

namespace vrperf {

struct PerfSnapshot {
    std::vector<SensorReading> readings;
    bool hwinfoConnected = false;
    bool windowsFallbackActive = false;
    double vrFrameTimeMs = 0.0;
    double vrDroppedFrames = 0.0;
    std::string statusText;
};

} // namespace vrperf
```

- [ ] **步骤 2：运行构建验证**

运行：

```powershell
cmake --build build --config Debug
```

预期：构建通过。

---

### 任务 6：实现 Windows fallback provider

**文件：**
- 创建：`src/data/windows_fallback_provider.h`
- 创建：`src/data/windows_fallback_provider.cpp`
- 修改：`CMakeLists.txt`
- 创建：`tests/test_windows_fallback_provider.cpp`
- 修改：`tests/CMakeLists.txt`

- [ ] **步骤 1：创建头文件**

创建 `src/data/windows_fallback_provider.h`：

```cpp
#pragma once

#include "hwinfo/sensor_data.h"

#include <Windows.h>
#include <vector>

namespace vrperf {

class WindowsFallbackProvider {
public:
    bool Refresh();
    std::vector<SensorReading> GetReadings() const;

private:
    bool hasPreviousCpuTimes_ = false;
    FILETIME previousIdleTime_{};
    FILETIME previousKernelTime_{};
    FILETIME previousUserTime_{};
    std::vector<SensorReading> readings_;

    static unsigned long long ToUInt64(const FILETIME& ft);
};

} // namespace vrperf
```

- [ ] **步骤 2：实现 CPU/RAM 读取**

创建 `src/data/windows_fallback_provider.cpp`：

```cpp
#include "data/windows_fallback_provider.h"

namespace vrperf {

unsigned long long WindowsFallbackProvider::ToUInt64(const FILETIME& ft)
{
    return (static_cast<unsigned long long>(ft.dwHighDateTime) << 32)
         | static_cast<unsigned long long>(ft.dwLowDateTime);
}

bool WindowsFallbackProvider::Refresh()
{
    readings_.clear();

    FILETIME idleTime;
    FILETIME kernelTime;
    FILETIME userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        if (hasPreviousCpuTimes_) {
            const auto idle = ToUInt64(idleTime) - ToUInt64(previousIdleTime_);
            const auto kernel = ToUInt64(kernelTime) - ToUInt64(previousKernelTime_);
            const auto user = ToUInt64(userTime) - ToUInt64(previousUserTime_);
            const auto total = kernel + user;
            if (total > 0) {
                SensorReading cpu;
                cpu.category = SensorCategory::CpuLoad;
                cpu.label = "CPU Load";
                cpu.value = 100.0 * static_cast<double>(total - idle) /
                            static_cast<double>(total);
                cpu.unit = "%";
                readings_.push_back(cpu);
            }
        }

        previousIdleTime_ = idleTime;
        previousKernelTime_ = kernelTime;
        previousUserTime_ = userTime;
        hasPreviousCpuTimes_ = true;
    }

    MEMORYSTATUSEX mem = {};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        SensorReading ram;
        ram.category = SensorCategory::RamUsage;
        ram.label = "RAM";
        ram.value = static_cast<double>(mem.dwMemoryLoad);
        ram.unit = "%";
        readings_.push_back(ram);
    }

    return !readings_.empty();
}

std::vector<SensorReading> WindowsFallbackProvider::GetReadings() const
{
    return readings_;
}

} // namespace vrperf
```

- [ ] **步骤 3：加入 CMake**

在根 `CMakeLists.txt` 的 `add_library(vr_perf_core STATIC ...)` 中加入：

```cmake
src/data/windows_fallback_provider.cpp
```

- [ ] **步骤 4：创建测试**

创建 `tests/test_windows_fallback_provider.cpp`：

```cpp
#include "data/windows_fallback_provider.h"

#include <cassert>
#include <iostream>

int main()
{
    vrperf::WindowsFallbackProvider provider;
    provider.Refresh();
    Sleep(20);
    assert(provider.Refresh());

    auto readings = provider.GetReadings();
    assert(!readings.empty());

    bool hasRam = false;
    for (const auto& reading : readings) {
        if (reading.category == vrperf::SensorCategory::RamUsage) {
            hasRam = true;
            assert(reading.value >= 0.0);
            assert(reading.value <= 100.0);
        }
    }

    assert(hasRam);
    std::cout << "[PASS] Windows fallback provider test passed\n";
    return 0;
}
```

- [ ] **步骤 5：注册测试**

在 `tests/CMakeLists.txt` 追加：

```cmake
add_executable(test_windows_fallback_provider
    test_windows_fallback_provider.cpp
)

target_link_libraries(test_windows_fallback_provider PRIVATE
    vr_perf_core
)

add_test(NAME WindowsFallbackProvider COMMAND test_windows_fallback_provider)
```

- [ ] **步骤 6：运行测试**

运行：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

预期：新增测试通过。

---

### 任务 7：App 中接入 HWiNFO 主源和 Windows 兜底

**文件：**
- 修改：`src/core/app.h`
- 修改：`src/core/app.cpp`

- [ ] **步骤 1：加入 fallback provider 成员**

在 `src/core/app.h` include 区域加入：

```cpp
#include "data/windows_fallback_provider.h"
```

在 private 成员区域加入：

```cpp
WindowsFallbackProvider windowsFallbackProvider_;
```

- [ ] **步骤 2：修改 UpdateOverlay 数据选择逻辑**

在 `App::UpdateOverlay()` 中，把读取数据逻辑替换为：

```cpp
hwinfoReader_.Refresh();
auto readings = hwinfoReader_.GetReadings();

if (readings.empty()) {
    windowsFallbackProvider_.Refresh();
    readings = windowsFallbackProvider_.GetReadings();
}
```

- [ ] **步骤 3：运行测试**

运行：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

预期：全部测试通过。

---

## 里程碑 4：加入 VR 专属性能指标

### 任务 8：实现 OpenVR frame timing 读取

**文件：**
- 创建：`src/vr/openvr_frame_timing.h`
- 创建：`src/vr/openvr_frame_timing.cpp`
- 修改：`CMakeLists.txt`
- 修改：`src/core/app.h`
- 修改：`src/core/app.cpp`

- [ ] **步骤 1：创建头文件**

创建 `src/vr/openvr_frame_timing.h`：

```cpp
#pragma once

#include <openvr.h>

namespace vrperf {

struct OpenVrFrameTimingSnapshot {
    bool available = false;
    double frameTimeMs = 0.0;
    double gpuFrameTimeMs = 0.0;
    uint32_t droppedFrames = 0;
};

class OpenVrFrameTiming {
public:
    OpenVrFrameTimingSnapshot Read() const;
};

} // namespace vrperf
```

- [ ] **步骤 2：实现读取逻辑**

创建 `src/vr/openvr_frame_timing.cpp`：

```cpp
#include "vr/openvr_frame_timing.h"

namespace vrperf {

OpenVrFrameTimingSnapshot OpenVrFrameTiming::Read() const
{
    OpenVrFrameTimingSnapshot snapshot;
    auto* compositor = vr::VRCompositor();
    if (!compositor) {
        return snapshot;
    }

    vr::Compositor_FrameTiming timing = {};
    timing.m_nSize = sizeof(timing);
    if (!compositor->GetFrameTiming(&timing, 0)) {
        return snapshot;
    }

    snapshot.available = true;
    snapshot.frameTimeMs = timing.m_flSystemTimeInSeconds * 1000.0;
    snapshot.gpuFrameTimeMs = timing.m_flPreSubmitGpuMs
                            + timing.m_flPostSubmitGpuMs
                            + timing.m_flCompositorRenderGpuMs;
    snapshot.droppedFrames = timing.m_nNumDroppedFrames;
    return snapshot;
}

} // namespace vrperf
```

- [ ] **步骤 3：加入 CMake**

在根 `CMakeLists.txt` 的 `add_library(vr_perf_core STATIC ...)` 中加入：

```cmake
src/vr/openvr_frame_timing.cpp
```

- [ ] **步骤 4：接入 App**

在 `src/core/app.h` include 区域加入：

```cpp
#include "vr/openvr_frame_timing.h"
```

在成员区域加入：

```cpp
OpenVrFrameTiming openVrFrameTiming_;
```

在 `App::UpdateOverlay()` 中读取：

```cpp
auto frameTiming = openVrFrameTiming_.Read();
```

当前阶段先不显示该数据，只保证读取路径不会破坏主流程。

- [ ] **步骤 5：运行构建验证**

运行：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

预期：构建和测试通过。

---

### 任务 9：在 UI 中显示 VR frame timing 状态

**文件：**
- 修改：`src/render/d2d_renderer.h`
- 修改：`src/render/d2d_renderer.cpp`
- 修改：`src/core/app.cpp`

- [ ] **步骤 1：扩展 DrawSensorPanel 签名**

把 `src/render/d2d_renderer.h` 中的声明改为：

```cpp
void DrawSensorPanel(const std::vector<SensorReading>& readings,
                     const Config& config,
                     double vrFrameTimeMs = 0.0,
                     uint32_t vrDroppedFrames = 0);
```

同步修改 `src/render/d2d_renderer.cpp` 中定义。

- [ ] **步骤 2：绘制 VR 指标行**

在 `DrawSensorPanel` 标题和分隔线之后加入：

```cpp
if (vrFrameTimeMs > 0.0) {
    std::wstringstream vrLine;
    vrLine << L"VR Frame: " << std::fixed << std::setprecision(1)
           << vrFrameTimeMs << L" ms  Drops: " << vrDroppedFrames;
    D2D1_RECT_F vrRect = D2D1::RectF(padding, y, w - padding, y + 28.0f);
    DrawText(vrLine.str(), vrRect, smallFormat_, textSecondaryBrush_);
    y += 32.0f;
}
```

- [ ] **步骤 3：从 App 传入数据**

在 `App::UpdateOverlay()` 中调用：

```cpp
d2dRenderer_.DrawSensorPanel(
    readings,
    config_,
    frameTiming.available ? frameTiming.gpuFrameTimeMs : 0.0,
    frameTiming.droppedFrames);
```

- [ ] **步骤 4：运行构建验证**

运行：

```powershell
cmake --build build --config Debug
```

预期：构建通过。

---

## 里程碑 5：安全边界、文档和手动验收

### 任务 10：记录安全边界和运行要求

**文件：**
- 修改：`README.md`
- 修改：`docs/research-notes.md`

- [ ] **步骤 1：README 增加安全边界**

在 `README.md` 的 Architecture 后加入：

```markdown
## Anti-Cheat Safety Boundary

VR Performance Profiler runs as a standalone SteamVR overlay application.
It does not inject into games, hook graphics APIs, read or write game process
memory, or install kernel drivers. Overlay content is submitted to SteamVR via
OpenVR `IVROverlay::SetOverlayTexture`.
```

- [ ] **步骤 2：README 增加 HWiNFO 说明**

在 Requirements 的 HWiNFO 条目下补充：

```markdown
HWiNFO data is read through its documented shared memory interface in read-only
mode. If HWiNFO is unavailable, the app falls back to basic Windows CPU/RAM
metrics.
```

- [ ] **步骤 3：运行 Markdown 人工检查**

打开并阅读：

```powershell
Get-Content README.md -Encoding UTF8
Get-Content docs\research-notes.md -Encoding UTF8
```

预期：文档没有重复冲突，安全边界描述和代码路线一致。

---

### 任务 11：手动 VR 验收

**文件：**
- 不改文件。

- [ ] **步骤 1：准备环境**

确认：

```text
SteamVR 已启动
HWiNFO64 已启动并启用 Shared Memory
至少一个 VR 头显被 SteamVR 识别
```

- [ ] **步骤 2：运行 Debug 版本**

运行：

```powershell
.\build\Debug\vr_perf_profiler.exe
```

- [ ] **步骤 3：验收 HUD 模式**

预期：

```text
SteamVR 中出现 VR Performance Profiler overlay
overlay 固定在 HMD 前方
显示 CPU/GPU/RAM 数据
Ctrl+Shift+H 可以隐藏/显示
```

- [ ] **步骤 4：验收 wrist 模式**

按下：

```text
Ctrl+Shift+M
```

预期：

```text
overlay 切换到控制器相对位置
再次按 Ctrl+Shift+M 切回 HUD
```

- [ ] **步骤 5：验收 HWiNFO 不可用降级**

关闭 HWiNFO 后重新运行：

```powershell
.\build\Debug\vr_perf_profiler.exe
```

预期：

```text
overlay 仍然显示
至少显示 CPU Load 和 RAM 指标
程序不崩溃
```

---

## 自审

**规格覆盖：**
- VRCX 同类显示方式：任务 1、2、3、9 覆盖。
- OpenVR overlay 安全边界：任务 3、10 覆盖。
- 不注入、不 Hook、不读写游戏内存：任务 10 覆盖。
- HWiNFO 主数据源：现有 `HwInfoReader` 已覆盖，任务 7 接入 App 层兜底。
- HWiNFO 不可用时可显示基础数据：任务 6、7、11 覆盖。
- VR 性能指标：任务 8、9 覆盖。
- 可验证性：任务 4、6 增加自动测试；任务 11 增加真实 VR 手动验收。

**占位符扫描：**
- 本计划没有使用未定内容标记或空泛执行描述。
- 每个新增文件均给出具体内容或具体修改位置。

**类型一致性：**
- `D2DRenderer::SetTargetTexture` 在任务 1 定义并在 `App::Initialize` 中调用。
- `WindowsFallbackProvider` 在任务 6 定义并在任务 7 中作为成员使用。
- `OpenVrFrameTiming` 在任务 8 定义并在任务 9 中传入 UI。
- `OverlayPositioner::MakeHudTransform` 和 `MakeWristTransform` 在任务 4 中从 private 移到 public，测试可以直接调用。
