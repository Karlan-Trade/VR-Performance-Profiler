#include "core/app.h"
#include "core/log.h"
#include "core/single_instance_guard.h"
#include <Windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
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
