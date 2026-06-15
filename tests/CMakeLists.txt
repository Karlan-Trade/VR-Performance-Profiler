cmake_minimum_required(VERSION 3.20)

# Simple test executable (no external test framework needed for now)
add_executable(test_hwinfo_reader
    test_hwinfo_reader.cpp
)

target_link_libraries(test_hwinfo_reader PRIVATE
    vr_perf_core
)

add_test(NAME HwInfoReader COMMAND test_hwinfo_reader)

add_executable(test_config
    test_config.cpp
)

target_link_libraries(test_config PRIVATE
    vr_perf_core
)

add_test(NAME Config COMMAND test_config)

add_executable(test_overlay_positioner
    test_overlay_positioner.cpp
)

target_link_libraries(test_overlay_positioner PRIVATE
    vr_perf_core
)

add_custom_command(TARGET test_overlay_positioner POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${OPENVR_BIN_DIR}/openvr_api.dll"
        $<TARGET_FILE_DIR:test_overlay_positioner>
    COMMENT "Copying openvr_api.dll for OverlayPositioner test"
)

add_test(NAME OverlayPositioner COMMAND test_overlay_positioner)

add_executable(test_windows_fallback_provider
    test_windows_fallback_provider.cpp
)

target_link_libraries(test_windows_fallback_provider PRIVATE
    vr_perf_core
)

add_test(NAME WindowsFallbackProvider COMMAND test_windows_fallback_provider)

add_executable(test_metric_aggregator
    test_metric_aggregator.cpp
)

target_link_libraries(test_metric_aggregator PRIVATE
    vr_perf_core
)

add_test(NAME MetricAggregator COMMAND test_metric_aggregator)

add_executable(test_libre_hardware_monitor_bridge_provider
    test_libre_hardware_monitor_bridge_provider.cpp
)

target_link_libraries(test_libre_hardware_monitor_bridge_provider PRIVATE
    vr_perf_core
)

add_test(NAME LibreHardwareMonitorBridgeProvider COMMAND test_libre_hardware_monitor_bridge_provider)
