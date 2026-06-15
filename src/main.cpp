#include "core/app.h"
#include <Windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
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
