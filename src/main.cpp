#include "core/app.h"
#include "core/log.h"
#include "core/single_instance_guard.h"
#include <Windows.h>
#include <openvr.h>

#include <chrono>
#include <cstring>
#include <iostream>

namespace {

void EnableDpiAwareness()
{
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) {
        return;
    }

    using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    auto setProcessDpiAwarenessContext =
        reinterpret_cast<SetProcessDpiAwarenessContextFn>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
    if (setProcessDpiAwarenessContext &&
        setProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        return;
    }

    using SetProcessDPIAwareFn = BOOL(WINAPI*)();
    auto setProcessDPIAware =
        reinterpret_cast<SetProcessDPIAwareFn>(
            GetProcAddress(user32, "SetProcessDPIAware"));
    if (setProcessDPIAware) {
        setProcessDPIAware();
    }
}

bool IsSteamVrInitProbeCommand(LPSTR commandLine)
{
    return commandLine && std::strstr(commandLine, "--steamvr-init-probe") != nullptr;
}

int RunSteamVrInitProbe()
{
    const auto start = std::chrono::steady_clock::now();
    vr::EVRInitError error = vr::VRInitError_None;
    vr::VR_Init(&error, vr::VRApplication_Overlay);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    if (error != vr::VRInitError_None) {
        std::cout << "VR_Init failed after " << elapsed.count() << " ms: "
                  << vr::VR_GetVRInitErrorAsEnglishDescription(error) << "\n";
        return 1;
    }

    std::cout << "VR_Init succeeded after " << elapsed.count() << " ms\n";
    vr::VR_Shutdown();
    return 0;
}

} // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    if (IsSteamVrInitProbeCommand(lpCmdLine)) {
        return RunSteamVrInitProbe();
    }

    EnableDpiAwareness();

    vrperf::SingleInstanceGuard singleInstance(
        L"Local\\VRPerfProfiler.SingleInstance");
    if (!singleInstance.IsAcquired()) {
        vrperf::LogInfo("Another VR Performance Profiler instance is already running");
        return 0;
    }

    vrperf::App app;

    if (!app.Initialize()) {
        MessageBoxA(nullptr, "Failed to initialize VR Performance Profiler.",
                    "VR Perf Profiler", MB_OK | MB_ICONERROR);
        return 1;
    }

    app.Run();
    app.Shutdown();

    return 0;
}
