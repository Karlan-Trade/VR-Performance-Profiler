# VR Performance Profiler

[English README](README.md)

VR Performance Profiler 是一个独立的 SteamVR/OpenVR 覆盖层性能监控工具，用于在 VR 内显示实时硬件监控数据和 SteamVR 帧时间数据。

## 功能

- 实时硬件监控：CPU/GPU 使用率、温度、频率、内存占用等
- VR 帧数监控：FPS、帧间隔、GPU 帧时间、头显刷新率、丢帧数
- SteamVR 覆盖层：支持 HUD 视角固定模式和手腕模式
- MSI Afterburner 优先数据源：可读取硬件监控共享内存
- 内置温度桥接工具：通过 LibreHardwareMonitor 读取 CPU/GPU 温度
- 可选 HWiNFO64 集成：启用共享内存后可读取更多传感器
- 可配置显示项：设置界面中按传感器行勾选要显示的数据
- 单实例运行：重复启动会直接退出，避免多个托盘/桥接进程并存

## 运行要求

- Windows 10/11 x64
- SteamVR
- WebView2 Runtime：设置界面需要。多数 Windows 11/Edge 环境已自带
- 可选：MSI Afterburner，用于更完整的硬件监控数据
- 可选：HWiNFO64，用于额外传感器数据

安装版不包含 SteamVR。SteamVR 需要用户自行安装。

## 数据源优先级

程序按以下顺序使用第一个可用硬件数据源：

1. MSI Afterburner
2. LibreHardwareMonitor bridge
3. HWiNFO64
4. Windows 基础 CPU/RAM 指标

当前不会混合多个数据源补齐缺失字段。SteamVR 帧数/帧时间数据作为虚拟传感器行加入设置界面，可单独勾选显示。

LibreHardwareMonitor bridge 会把快照写入：

```text
%LOCALAPPDATA%\VRPerfProfiler\lhm-sensors.json
```

## 安装版

运行发布包中的：

```text
VRPerformanceProfiler-<version>-Setup.exe
```

安装器会：

- 允许用户选择安装位置
- 默认安装到 `%LOCALAPPDATA%\Programs\VR Performance Profiler`
- 如果选择的目录不是 `VR Performance Profiler`，会在该目录下创建产品子目录
- 安装主程序、OpenVR DLL、WebView2 loader、自包含的 LibreHardwareMonitor bridge
- 创建桌面快捷方式、开始菜单快捷方式和卸载项
- 安装完成后启动程序

卸载器只删除安装清单记录的文件，不会递归删除用户选择的整个目录。

## 便携版

发布包也提供：

```text
VRPerformanceProfiler-<version>-portable.zip
```

解压后运行 `vr_perf_profiler.exe` 即可。便携版仍需要系统中已安装 SteamVR。

## 使用方式

1. 启动 SteamVR
2. 可选：启动 MSI Afterburner
3. 可选：启动 HWiNFO64 并开启 Shared Memory
4. 运行 `vr_perf_profiler.exe`
5. 在设置窗口中勾选要显示的传感器和 VR 帧数指标

程序会在系统托盘运行。托盘菜单提供：

- `Connect SteamVR Overlay`：手动连接 SteamVR 覆盖层
- `Settings...`：打开设置界面
- `Exit`：退出程序

## 快捷键

- `Ctrl+Shift+H`：显示/隐藏覆盖层
- `Ctrl+Shift+M`：切换 HUD/手腕模式

## 帧数检测说明

帧数指标来自 OpenVR compositor timing API。它是外部 SteamVR 运行时测量，不注入游戏、不 Hook DirectX/OpenGL/Vulkan、不读取游戏内存。

因此它更适合保持反作弊边界，但数值可能和游戏内部 FPS 计数器或引擎 profiler 有差异。

## 构建

构建要求：

1. CMake 3.20 或更高版本
2. Visual Studio 2022，安装 C++ Desktop Development 工作负载
3. .NET 8 SDK 或更高版本
4. SteamVR/OpenVR 运行环境

示例：

```powershell
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

## 打包

从 Visual Studio x64 Developer Command Prompt 运行：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\package_installer.ps1
```

输出目录：

```text
dist\
```

打包脚本会生成安装器和便携 ZIP。安装器不会打包 SteamVR。

## 反作弊边界

VR Performance Profiler 是独立 SteamVR 覆盖层应用：

- 不注入游戏进程
- 不 Hook 图形 API
- 不读写游戏内存
- 不安装内核驱动
- 通过 OpenVR `IVROverlay::SetOverlayTexture` 提交覆盖层画面
- 通过 OpenVR compositor timing API 读取帧数相关指标

## 许可证

本项目使用 MIT License。详见 `LICENSE`。

第三方依赖声明见 `THIRD_PARTY_NOTICES.md`。
