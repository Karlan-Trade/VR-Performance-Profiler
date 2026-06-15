cmake_minimum_required(VERSION 3.20)
project(VRPerfProfiler VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ──────────────────────────────────────────────────────────────
# Dependencies via FetchContent
# ──────────────────────────────────────────────────────────────
include(FetchContent)

# OpenVR SDK
FetchContent_Declare(
    openvr
    GIT_REPOSITORY https://github.com/ValveSoftware/openvr.git
    GIT_TAG        v2.5.1
)
FetchContent_MakeAvailable(openvr)

# nlohmann/json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

find_program(DOTNET_EXECUTABLE
    NAMES dotnet dotnet.exe
    PATHS "C:/Program Files/dotnet"
)

if(CMAKE_BUILD_TYPE)
    set(LHM_BRIDGE_CONFIGURATION "${CMAKE_BUILD_TYPE}")
else()
    set(LHM_BRIDGE_CONFIGURATION "Debug")
endif()

# ──────────────────────────────────────────────────────────────
# OpenVR paths
# ──────────────────────────────────────────────────────────────
set(OPENVR_INCLUDE_DIR "${openvr_SOURCE_DIR}/headers")
set(OPENVR_LIB_DIR     "${openvr_SOURCE_DIR}/lib/win64")
set(OPENVR_BIN_DIR     "${openvr_SOURCE_DIR}/bin/win64")

# ──────────────────────────────────────────────────────────────
# Core static library (shared by app and tests)
# ──────────────────────────────────────────────────────────────
add_library(vr_perf_core STATIC
    src/core/app.cpp
    src/core/config.cpp
    src/data/libre_hardware_monitor_bridge_provider.cpp
    src/data/metric_aggregator.cpp
    src/data/windows_fallback_provider.cpp
    src/hwinfo/hwinfo_reader.cpp
    src/overlay/overlay_manager.cpp
    src/overlay/overlay_positioner.cpp
    src/render/d3d11_renderer.cpp
    src/render/d2d_renderer.cpp
    src/vr/openvr_frame_timing.cpp
)

target_include_directories(vr_perf_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OPENVR_INCLUDE_DIR}
)

target_compile_definitions(vr_perf_core PUBLIC
    UNICODE
    _UNICODE
    NOMINMAX
)

target_link_libraries(vr_perf_core PUBLIC
    ${OPENVR_LIB_DIR}/openvr_api.lib
    nlohmann_json::nlohmann_json
    d3d11.lib
    d2d1.lib
    dwrite.lib
    dxgi.lib
    ole32.lib
    uuid.lib
    windowsapp.lib
)

# ──────────────────────────────────────────────────────────────
# Main executable
# ──────────────────────────────────────────────────────────────
add_executable(vr_perf_profiler WIN32
    src/main.cpp
    src/ui/tray_icon.cpp
    src/ui/settings_window.cpp
    resources/app.rc
    resources/settings_dialog.rc
)

target_link_libraries(vr_perf_profiler PRIVATE
    vr_perf_core
    shell32.lib
)

# Copy openvr_api.dll to output
add_custom_command(TARGET vr_perf_profiler POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${OPENVR_BIN_DIR}/openvr_api.dll"
        $<TARGET_FILE_DIR:vr_perf_profiler>
    COMMENT "Copying openvr_api.dll to output directory"
)

if(DOTNET_EXECUTABLE)
    add_custom_command(TARGET vr_perf_profiler POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E env
            "DOTNET_CLI_HOME=${CMAKE_CURRENT_SOURCE_DIR}/.dotnet-home"
            "NUGET_PACKAGES=${CMAKE_CURRENT_SOURCE_DIR}/.nuget-packages"
            "APPDATA=${CMAKE_CURRENT_BINARY_DIR}/appdata"
            "${DOTNET_EXECUTABLE}" publish
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/lhm_bridge/VRPerfProfiler.LhmBridge.csproj"
            -c "${LHM_BRIDGE_CONFIGURATION}"
            -o "$<TARGET_FILE_DIR:vr_perf_profiler>/lhm_bridge"
            --no-self-contained
        COMMENT "Publishing LibreHardwareMonitor bridge helper"
    )
else()
    message(WARNING "dotnet SDK not found; LibreHardwareMonitor bridge helper will not be built")
endif()

# ──────────────────────────────────────────────────────────────
# Tests
# ──────────────────────────────────────────────────────────────
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
