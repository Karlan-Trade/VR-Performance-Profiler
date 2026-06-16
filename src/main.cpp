#include "core/app.h"
#include "core/log.h"
#include "core/single_instance_guard.h"
#include <Windows.h>

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

} // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
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
