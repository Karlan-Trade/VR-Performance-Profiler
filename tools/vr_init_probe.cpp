#include <openvr.h>

#include <chrono>
#include <iostream>

int main()
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
