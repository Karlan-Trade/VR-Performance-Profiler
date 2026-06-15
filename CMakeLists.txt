cmake_minimum_required(VERSION 3.20)
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(VRPerfProfiler VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

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

set(WEBVIEW2_VERSION "1.0.4022.49")
set(WEBVIEW2_NATIVE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/.nuget-packages/microsoft.web.webview2/${WEBVIEW2_VERSION}/build/native")
set(WEBVIEW2_INCLUDE_DIR "${WEBVIEW2_NATIVE_DIR}/include")
set(WEBVIEW2_ARCH_DIR "${WEBVIEW2_NATIVE_DIR}/x64")

if(NOT EXISTS "${WEBVIEW2_INCLUDE_DIR}/WebView2.h")
    set(WEBVIEW2_PACKAGE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/.nuget-packages/microsoft.web.webview2/${WEBVIEW2_VERSION}")
    set(WEBVIEW2_PACKAGE_FILE
        "${CMAKE_CURRENT_SOURCE_DIR}/.nuget-packages/microsoft.web.webview2/Microsoft.Web.WebView2.${WEBVIEW2_VERSION}.nupkg")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.nuget-packages/microsoft.web.webview2")
    message(STATUS "Microsoft.Web.WebView2 SDK not found; attempting NuGet package download")
    file(DOWNLOAD
        "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${WEBVIEW2_VERSION}"
        "${WEBVIEW2_PACKAGE_FILE}"
        STATUS WEBVIEW2_DOWNLOAD_STATUS
        SHOW_PROGRESS)
    list(GET WEBVIEW2_DOWNLOAD_STATUS 0 WEBVIEW2_DOWNLOAD_CODE)
    if(WEBVIEW2_DOWNLOAD_CODE EQUAL 0)
        file(MAKE_DIRECTORY "${WEBVIEW2_PACKAGE_DIR}")
        file(ARCHIVE_EXTRACT
            INPUT "${WEBVIEW2_PACKAGE_FILE}"
            DESTINATION "${WEBVIEW2_PACKAGE_DIR}")
    else()
        message(WARNING "Microsoft.Web.WebView2 SDK download failed: ${WEBVIEW2_DOWNLOAD_STATUS}")
    endif()
endif()

set(WEBVIEW2_AVAILABLE OFF)
if(EXISTS "${WEBVIEW2_INCLUDE_DIR}/WebView2.h" AND EXISTS "${WEBVIEW2_ARCH_DIR}/WebView2Loader.dll.lib")
    set(WEBVIEW2_AVAILABLE ON)
endif()

# ──────────────────────────────────────────────────────────────
# Core static library (shared by app and tests)
# ──────────────────────────────────────────────────────────────
add_library(vr_perf_core STATIC
    src/core/app.cpp
    src/core/config.cpp
    src/core/log.cpp
    src/core/single_instance_guard.cpp
    src/data/libre_hardware_monitor_bridge_provider.cpp
    src/data/metric_aggregator.cpp
    src/data/msi_afterburner_provider.cpp
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
    src/ui/web_settings_window.cpp
    resources/app.rc
)

target_link_libraries(vr_perf_profiler PRIVATE
    vr_perf_core
    shell32.lib
)

if(WEBVIEW2_AVAILABLE)
    target_compile_definitions(vr_perf_profiler PRIVATE VRPERF_WITH_WEBVIEW2)
    target_include_directories(vr_perf_profiler PRIVATE "${WEBVIEW2_INCLUDE_DIR}")
    target_link_libraries(vr_perf_profiler PRIVATE "${WEBVIEW2_ARCH_DIR}/WebView2Loader.dll.lib")
    add_custom_command(TARGET vr_perf_profiler POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WEBVIEW2_ARCH_DIR}/WebView2Loader.dll"
            $<TARGET_FILE_DIR:vr_perf_profiler>
        COMMENT "Copying WebView2Loader.dll to output directory"
    )
else()
    message(WARNING "Microsoft.Web.WebView2 SDK not found; falling back to Win32 settings window")
endif()

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
