# VR Performance Profiler — 技术调研文档

> 调研日期：2026-06-14
> 调研方式：基于 Claude 训练数据（截止 2025 年初），部分信息需自行验证

---

## 1. SteamVR Overlay 开发

### 1.1 OpenVR IVROverlay API

SteamVR overlay 通过 OpenVR SDK 中的 `IVROverlay` 接口实现。

**核心流程：**

```cpp
#include <openvr.h>

// 初始化 OpenVR（注意：Overlay 应用使用 VRApplication_Overlay 类型）
vr::EVRInitError error = vr::VRInitError_None;
vr::IVRSystem* system = vr::VR_Init(&error, vr::VRApplication_Overlay);

// 获取 Overlay 接口
vr::IVROverlay* overlay = vr::VROverlay();

// 创建 overlay
vr::VROverlayHandle_t handle;
overlay->CreateOverlay("my.overlay.name", "My Overlay Title", &handle);
```

**关键 IVROverlay 方法：**

| 方法 | 用途 |
|------|------|
| `CreateOverlay` | 创建新的 overlay |
| `DestroyOverlay` | 销毁 overlay |
| `SetOverlayFromFile` | 从图片文件设置 overlay 内容 |
| `SetOverlayTexture` | 从 GPU 纹理设置 overlay 内容 |
| `SetOverlayColor` | 设置颜色/透明度 |
| `SetOverlayAlpha` | 设置透明度 |
| `SetOverlayWidthInMeters` | 设置 overlay 在 VR 中的物理宽度 |
| `SetOverlayCurvature` | 设置曲面程度（0=平面，1=最大弯曲） |
| `SetOverlayInputMethod` | 设置输入交互模式 |
| `ShowOverlay` / `HideOverlay` | 显示/隐藏 |
| `SetOverlayIntersectionMask` | 设置点击检测区域 |

### 1.2 Overlay 定位模式

**a) HUD 模式（固定在视野中）**

```cpp
overlay->SetOverlayFlag(handle, vr::VROverlayFlags_HUD, true);
// overlay 固定在玩家头显前方，不随世界移动
vr::HmdMatrix34_t transform = { /* 4x3 矩阵 */ };
overlay->SetOverlayTransformTrackedDeviceRelative(
    handle,
    vr::k_unTrackedDeviceIndex_Hmd,  // 相对于头显
    &transform
);
```

**b) Dashboard 模式（悬浮面板）**

```cpp
overlay->SetOverlayDashboard(handle, true);
// SteamVR 自动管理位置，用户可以手动拖拽
```

**c) 世界空间模式（固定在物理空间）**

```cpp
vr::HmdMatrix34_t worldTransform = { /* 4x3 矩阵 */ };
overlay->SetOverlayTransformAbsolute(
    handle,
    vr::TrackingUniverseStanding,
    &worldTransform
);
```

**d) 相对于某个追踪设备（如手柄）**

```cpp
vr::HmdMatrix34_t offset = { /* 相对于设备的偏移 */ };
overlay->SetOverlayTransformTrackedDeviceRelative(
    handle,
    trackedDeviceIndex,  // 设备索引（手柄等）
    &offset
);
```

### 1.3 变换矩阵格式

```cpp
// HmdMatrix34_t — 4x3 行主序矩阵（3行4列）
struct HmdMatrix34_t {
    float m[3][4];
};
// 单位矩阵 + 平移：
// | 1 0 0 x |
// | 0 1 0 y |
// | 0 0 1 z |
```

### 1.4 渲染纹理提交

Overlay 支持多种纹理类型：

| 纹理类型 | 枚举值 | handle 类型 |
|---------|--------|------------|
| OpenGL | `TextureType_OpenGL` | `GLuint` 纹理 ID |
| DirectX 11 | `TextureType_DirectX` | `ID3D11ShaderResourceView*` |
| DirectX 12 | `TextureType_DirectX12` | `D3D12TextureData_t*` |
| Vulkan | `TextureType_Vulkan` | `VRVulkanTextureData_t*` |

**DX11 纹理提交示例：**

```cpp
vr::Texture_t texture;
texture.handle = d3d11ShaderResourceView;
texture.eType = vr::TextureType_DirectX;
texture.eColorSpace = vr::ColorSpace_Auto;
overlay->SetOverlayTexture(handle, &texture);
```

### 1.5 事件处理

```cpp
vr::VREvent_t event;
while (overlay->PollNextOverlayEvent(handle, &event, sizeof(event))) {
    switch (event.eventType) {
        case vr::VREvent_MouseMove:
            break;
        case vr::VREvent_MouseButtonDown:
            break;
        case vr::VREvent_OverlayShown:
            break;
    }
}
```

### 1.6 OpenVR SDK 状态（2026 年）

- **OpenVR SDK 仍然是 SteamVR overlay 开发的唯一成熟方案**
- OpenXR 的 overlay 扩展（`XR_EXTX_overlay`）仍处于实验阶段
- SteamVR 本身仍然基于 OpenVR
- SDK 获取：`https://github.com/ValveSoftware/openvr`

### 1.7 参考项目

| 项目 | 说明 | 语言 |
|------|------|------|
| **Desktop+** | 开源桌面 overlay，架构完整 | C++ |
| **VRCX** | 独立进程 + OpenVR overlay + 离屏 Web/CEF 渲染到纹理 | C# / CEF / OpenVR |
| **OVR Toolkit** | 商业 overlay 应用 | C++ |
| **XS Overlay** | 轻量级 overlay，低开销 | C++ |

### 1.8 本项目 overlay 显示架构决策

参考 VRCX 的实现方式，本项目数据显示采用同类安全边界：

```text
Profiler 主进程
  → 读取硬件/VR 性能数据
  → Direct2D 绘制 UI
  → Direct3D 11 纹理
  → OpenVR IVROverlay::SetOverlayTexture
  → SteamVR 内显示 HUD / 手腕 overlay
```

关键原则：

- 使用 SteamVR/OpenVR 官方 overlay 通道显示内容。
- 不注入 VR 游戏进程。
- 不 Hook DXGI / D3D11 / OpenGL / Vulkan 等游戏渲染链。
- 不读取或写入游戏进程内存。
- 不安装内核驱动。
- overlay 内容以独立纹理提交给 SteamVR，由 SteamVR compositor 负责显示。

这种方式和 VRCX 的 VR overlay 思路一致：应用只和 SteamVR/OpenVR 交互，不直接修改游戏画面。相比 RTSS 类 Hook overlay 或进程注入方案，误判为外挂的风险更低，也更符合 VR overlay 工具的工程边界。

---

## 2. 硬件监控方案

### 2.1 默认数据源策略（2026-06-15 修订）

HWiNFO64 不再作为默认必需项，而是作为可选增强源。默认数据链路改为：

```text
LibreHardwareMonitor bridge -> HWiNFO optional enrichment -> Windows fallback
```

**工程原因：**
- Windows 原生 API 没有可靠通用 CPU 温度接口。
- LibreHardwareMonitor 覆盖 CPU/GPU/主板/存储等温度传感器，适合补默认温度能力。
- LibreHardwareMonitor 是 .NET/C# 生态，直接嵌入 C++ overlay 主进程会引入运行时和权限边界复杂度。
- 当前实现采用 JSON bridge：未来 C# helper 使用 `LibreHardwareMonitorLib` 采集传感器并写入 `%LOCALAPPDATA%/VRPerfProfiler/lhm-sensors.json`，C++ 主进程只读取快照。

**桥接 JSON：**

```json
{
  "readings": [
    { "category": "cpu_temp", "label": "CPU Package", "value": 68.0, "unit": "C" },
    { "category": "gpu_temp", "label": "GPU Core", "value": 61.0, "unit": "C" }
  ]
}
```

### 2.2 HWiNFO64 共享内存（可选增强）

HWiNFO64 通过共享内存接口暴露传感器数据，应用程序可直接读取。

**优势：**
- 覆盖所有硬件（CPU、GPU、主板、硬盘等）
- 数据可靠，由专业工具采集
- 应用程序无需管理员权限
- 不需要直接对接厂商 API

**共享内存结构：** `HWiNFO_SENSORS_SHARED_MEM`
- 文档：`https://www.hwinfo.com/info/` (Shared Memory section)
- 需要 HWiNFO64 在后台运行

**典型使用流程：**
1. 打开 HWiNFO 共享内存映射文件
2. 读取传感器数量和数据结构
3. 定期刷新获取最新值

### 2.3 方案对比

| 方案 | 优势 | 劣势 |
|------|------|------|
| **LibreHardwareMonitor bridge** ✅ | 开源、覆盖 CPU/GPU 温度、适合默认温度 | 需要 helper，部分传感器可能需要管理员权限 |
| **HWiNFO 共享内存** | 全面、可靠、无需管理员 | 依赖 HWiNFO 运行 |
| **NvAPI / NVML** | 直接、快速 | 仅 NVIDIA |
| **AMD ADL** | 直接 | 仅 AMD、文档差 |
| **Windows PDH** | 零依赖 | 无温度数据、GPU 数据粒度粗 |
| **WMI** | 内建 | 不可靠、性能差 |

### 2.4 各指标最佳数据源

| 数据点 | 首选方案 | 备选方案 |
|--------|---------|---------|
| CPU 使用率（每核心） | Windows API / HWiNFO | LibreHardwareMonitor |
| CPU 温度 | LibreHardwareMonitor bridge | HWiNFO |
| GPU 使用率 | LibreHardwareMonitor bridge / 厂商 API | HWiNFO |
| GPU 温度 | LibreHardwareMonitor bridge / NvAPI / ADL | HWiNFO |
| GPU 显存 | HWiNFO / DXGI | NvAPI / ADL |
| 内存使用 | HWiNFO / `GlobalMemoryStatusEx` | PDH |
| 帧时间 | OpenVR `GetFrameTiming` | — |

### 2.5 MVP 数据接入结论

当前 MVP 数据层采用按指标合并，而不是单一主源：

- CPU Load：`GetSystemTimes` 计算两次采样之间的 busy ratio。
- RAM：`GlobalMemoryStatusEx` 读取系统内存使用率。
- CPU/GPU 温度：优先读取 LibreHardwareMonitor bridge JSON；HWiNFO 作为可选补充。

Windows fallback 只覆盖基础可用性，不替代温度、GPU、风扇、电压等硬件传感器能力。

---

## 3. 项目技术栈总结

| 组件 | 选择 | 说明 |
|------|------|------|
| 语言 | C++ | OpenVR 原生支持，性能最佳 |
| 构建系统 | CMake | 业界标准，易于管理依赖 |
| 数据来源 | LibreHardwareMonitor bridge + optional HWiNFO + Windows API | 温度默认化，HWiNFO 可选 |
| Overlay API | OpenVR IVROverlay | 唯一成熟方案 |
| 纹理渲染 | Direct2D → Direct3D 11 | Windows 原生，文字质量好 |
| 配置存储 | nlohmann/json | 轻量 JSON 库 |

---

## 4. 待验证事项

- [ ] OpenVR SDK 最新版本和 API 变化
- [ ] LibreHardwareMonitor helper 打包与权限行为
- [ ] HWiNFO 共享内存结构体最新定义
- [ ] Desktop+ 项目的 overlay 渲染管线实现
- [ ] D2D + D3D11 与 OpenVR 纹理提交的集成方式
- [ ] 手腕追踪模式下 overlay 的最佳定位参数
