# VR Performance Profiler

[English README](README.en.md)

VR Performance Profiler 是一个独立的 SteamVR/OpenVR 覆盖层性能监控工具，用于在 VR 内显示实时硬件监控数据和 SteamVR 帧时间数据。

## 功能

- 实时硬件监控：CPU/GPU 使用率、温度、频率、内存占用等
- VR 帧数监控：FPS、帧间隔、GPU 帧时间、头显刷新率、丢帧数
- SteamVR 覆盖层：支持 HUD 视角固定模式和手腕模式
- 硬件数据源：默认读取 MSI Afterburner 共享内存，也可在设置中切换为 HWiNFO
- 可配置显示项：设置界面中按传感器行勾选要显示的数据，可调 HUD 面板大小
- 显示优化：温度以正确的 `°C` 符号显示，HUD 显示范围与覆盖层范围一致，手腕界面默认更紧凑
- 设置界面主题：深浅主题会同步更新 Web 设置窗口内容和标题栏颜色
- 单实例运行：重复启动会直接退出，避免多个托盘进程并存

## 运行要求

- Windows 10/11 x64
- SteamVR
- WebView2 Runtime：设置界面需要。多数 Windows 11/Edge 环境已自带
- MSI Afterburner：默认硬件监控数据来源，需要启用 Hardware Monitoring / shared memory
- HWiNFO：可选硬件监控数据来源，需要开启 Shared Memory Support

安装版不包含 SteamVR。SteamVR 需要用户自行安装。

## 数据源

硬件传感器默认读取 MSI Afterburner 的硬件监控共享内存。也可以在设置界面的“主数据来源”下拉框中切换为 HWiNFO，共享内存不可用时硬件行会为空。

SteamVR 帧数/帧时间数据作为虚拟传感器行加入设置界面，可单独勾选显示。

## 安装版

运行发布包中的：

```text
VRPerformanceProfiler-<version>-Setup.msi
```

安装器会：

- 允许用户选择安装位置
- 默认安装到 `%LOCALAPPDATA%\Programs\VR Performance Profiler`
- 安装主程序、OpenVR DLL、WebView2 loader
- 创建开始菜单快捷方式和标准卸载项

安装器由 WiX Toolset 生成标准 Windows Installer MSI，不再发布压缩自解压 EXE 安装器。
为避免部分自定义安装盘符上 `Config.Msi` 回滚目录权限异常，MSI 会禁用 Windows Installer 回滚。

## 便携版

发布包也提供：

```text
VRPerformanceProfiler-<version>-portable.zip
```

解压后运行 `vr_perf_profiler.exe` 即可。便携版仍需要系统中已安装 SteamVR。

## 使用方式

1. 启动 SteamVR
2. 启动 MSI Afterburner，并确认 Hardware Monitoring / shared memory 可用；或启动 HWiNFO 并开启 Shared Memory Support
3. 运行 `vr_perf_profiler.exe`
4. 设置窗口会在启动后显示；选择主数据来源，勾选要显示的传感器和 VR 帧数指标
5. 可按需要调整主题、更新间隔、HUD 面板大小，并在 HUD/手腕模式之间切换

程序会在系统托盘运行。托盘菜单提供：

- `Connect SteamVR Overlay`：手动连接 SteamVR 覆盖层
- `Settings...`：打开设置界面
- `Exit`：退出程序

## 快捷键

- `Ctrl+Shift+H`：显示/隐藏覆盖层
- `Ctrl+Shift+M`：切换 HUD/手腕模式

## 覆盖层显示

- HUD 模式固定在头显视野中，默认距离为 `1.0 m`
- HUD 面板大小可在 Web 设置界面中通过滑块调整，范围为 `0.50 m` 到 `2.50 m`
- 手腕模式附着在控制器上，默认宽度为 `0.75 m`

## 帧数检测说明

帧数指标来自 OpenVR compositor timing API。它是外部 SteamVR 运行时测量，不注入游戏、不 Hook DirectX/OpenGL/Vulkan、不读取游戏内存。

因此它更适合保持反作弊边界，但数值可能和游戏内部 FPS 计数器或引擎 profiler 有差异。

## 构建

构建要求：

1. CMake 3.20 或更高版本
2. Visual Studio 2022，安装 C++ Desktop Development 工作负载
3. SteamVR/OpenVR 运行环境
4. WiX Toolset 4，用于构建 MSI 安装器；只构建便携 ZIP 时可不安装

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
安装器由 WiX Toolset 4 生成；构建 MSI 时需要 `wix.exe` 在 PATH 中，或安装到仓库本地 `.tools\wix\wix.exe`。若只需要便携 ZIP，可添加 `-SkipMsi`。MSI 会禁用 Windows Installer 回滚，以避开部分自定义盘符上的 `Config.Msi` 权限问题。

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
