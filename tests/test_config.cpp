#include "core/config.h"
#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

int main()
{
    std::cout << "=== Config Tests ===" << std::endl;

    std::string testPath = "test_config.json";

    // Test 1: Default values
    {
        vrperf::Config config;
        config.Load(testPath);

        assert(config.overlay.mode == "hud");
        assert(config.overlay.widthMeters == 1.5f);
        assert(config.overlay.alpha == 0.85f);
        assert(config.version == 5);
        assert(config.overlay.autoConnectVr == false);
        assert(config.hud.pitchDegrees == 0.0f);
        assert(config.hud.distanceMeters == 1.0f);
        assert(config.wrist.hand == "left");
        assert(config.wrist.widthMeters == 0.75f);
        assert(config.metrics.size() == 13);
        assert(config.appearance.theme == "dark");
        assert(config.general.language == "zh");

        std::cout << "[PASS] Default values correct" << std::endl;
    }

    // Test 2: Save and reload
    {
        vrperf::Config config;
        config.Load(testPath);

        // Modify some values
        config.overlay.mode = "wrist";
        config.overlay.widthMeters = 1.25f;
        config.overlay.alpha = 0.9f;
        config.overlay.updateIntervalMs = 1000;
        config.appearance.theme = "light";
        config.general.language = "en";

        // Disable a metric
        for (auto& m : config.metrics) {
            if (m.category == "gpu_clock") {
                m.enabled = true;
                m.sensorKey = "v2|4D53492041667465726275726E6572|gpu_clock|1|32|436F726520636C6F636B";
                m.source = "MSI Afterburner";
                m.sensorId = 1;
                m.readingId = 32;
            }
        }

        assert(config.Save(testPath));

        // Reload and verify
        vrperf::Config config2;
        config2.Load(testPath);

        assert(config2.overlay.mode == "wrist");
        assert(config2.overlay.widthMeters == 1.25f);
        assert(config2.overlay.alpha == 0.9f);
        assert(config2.overlay.updateIntervalMs == 1000);
        assert(config2.appearance.theme == "light");
        assert(config2.general.language == "en");

        for (const auto& m : config2.metrics) {
            if (m.category == "gpu_clock") {
                assert(m.enabled == true);
                assert(m.sensorKey == "v2|4D53492041667465726275726E6572|gpu_clock|1|32|436F726520636C6F636B");
                assert(m.source == "MSI Afterburner");
                assert(m.sensorId == 1);
                assert(m.readingId == 32);
            }
        }

        std::cout << "[PASS] Save and reload test passed" << std::endl;
    }

    // Test 3: JSON structure
    {
        std::ifstream file(testPath);
        nlohmann::json j = nlohmann::json::parse(file);

        assert(j.contains("version"));
        assert(j.contains("overlay"));
        assert(j.contains("hud"));
        assert(j.contains("wrist"));
        assert(j.contains("metrics"));
        assert(j.contains("appearance"));
        assert(j.contains("hotkeys"));
        assert(j.contains("general"));
        assert(j["overlay"]["width_meters"] == 1.25f);

        assert(j["metrics"].is_array());
        assert(j["metrics"].size() == 13);

        std::cout << "[PASS] JSON structure valid" << std::endl;
    }

    // Test 4: v1 configs migrate to automatic SteamVR overlay connection.
    {
        nlohmann::json oldConfig = {
            {"version", 1},
            {"overlay", {
                {"mode", "hud"},
                {"auto_connect_vr", false}
            }}
        };

        {
            std::ofstream file(testPath);
            file << oldConfig.dump(4);
        }

        vrperf::Config config;
        config.Load(testPath);

        assert(config.version == 5);
        assert(config.overlay.autoConnectVr == false);
        assert(config.overlay.widthMeters == 1.5f);
        assert(config.hud.pitchDegrees == 0.0f);
        assert(config.hud.distanceMeters == 1.0f);
        assert(config.wrist.widthMeters == 0.75f);

        std::cout << "[PASS] v1 auto-connect migration passed" << std::endl;
    }

    // Test 5: v2 configs using the old tiny HUD defaults migrate to
    // VRCX-like visible HUD defaults.
    {
        nlohmann::json oldConfig = {
            {"version", 2},
            {"overlay", {
                {"mode", "hud"},
                {"width_meters", 0.3f},
                {"auto_connect_vr", true}
            }},
            {"hud", {
                {"pitch_degrees", -15.0f},
                {"distance_meters", 1.0f}
            }},
            {"wrist", {
                {"width_meters", 0.12f},
                {"offset_x", 0.05f},
                {"offset_y", 0.02f},
                {"offset_z", -0.05f},
                {"tilt_x", -30.0f}
            }}
        };

        {
            std::ofstream file(testPath);
            file << oldConfig.dump(4);
        }

        vrperf::Config config;
        config.Load(testPath);

        assert(config.version == 5);
        assert(config.overlay.widthMeters == 1.5f);
        assert(config.hud.pitchDegrees == 0.0f);
        assert(config.hud.distanceMeters == 1.0f);
        assert(config.wrist.widthMeters == 0.75f);
        assert(config.wrist.offsetX == 0.0f);
        assert(config.wrist.offsetY == 0.0f);
        assert(config.wrist.offsetZ == 0.0f);
        assert(config.wrist.tiltX == 0.0f);

        std::cout << "[PASS] v2 HUD visibility migration passed" << std::endl;
    }

    // Test 6: wrist placement is pinned to the compact baseline even if an
    // already-current config contains stale experimental wrist settings.
    {
        nlohmann::json currentConfig = {
            {"version", 4},
            {"wrist", {
                {"hand", "right"},
                {"width_meters", 0.12f},
                {"offset_x", 0.05f},
                {"offset_y", 0.02f},
                {"offset_z", -0.05f},
                {"tilt_x", -30.0f},
                {"tilt_y", 10.0f},
                {"tilt_z", 5.0f}
            }}
        };

        {
            std::ofstream file(testPath);
            file << currentConfig.dump(4);
        }

        vrperf::Config config;
        config.Load(testPath);

        assert(config.version == 5);
        assert(config.overlay.autoConnectVr == false);
        assert(config.wrist.hand == "right");
        assert(config.wrist.widthMeters == 0.75f);
        assert(config.wrist.offsetX == 0.0f);
        assert(config.wrist.offsetY == 0.0f);
        assert(config.wrist.offsetZ == 0.0f);
        assert(config.wrist.tiltX == 0.0f);
        assert(config.wrist.tiltY == 0.0f);
        assert(config.wrist.tiltZ == 0.0f);

        std::cout << "[PASS] v4 wrist normalization passed" << std::endl;
    }

    // Test 7: sensor strings from native providers may contain local-codepage
    // bytes. Saving config must normalize them instead of aborting in JSON dump.
    {
        vrperf::Config config;
        config.Load(testPath);
        config.metrics.clear();

        std::string invalidBytes = "sensor";
        invalidBytes.push_back(static_cast<char>(0xFF));
        invalidBytes += "label";

        vrperf::MetricConfig metric;
        metric.category = "gpu_temp";
        metric.enabled = true;
        metric.label = invalidBytes;
        metric.source = invalidBytes;
        metric.sensorKey = invalidBytes;
        metric.sensorId = 0;
        metric.readingId = 1;
        config.metrics.push_back(metric);

        assert(config.Save(testPath));
        std::ifstream file(testPath);
        const auto parsed = nlohmann::json::parse(file);
        assert(parsed["metrics"].is_array());
        assert(parsed["metrics"].size() == 1);
        assert(parsed["metrics"][0]["label"].is_string());

        std::cout << "[PASS] invalid UTF-8 config save normalization passed" << std::endl;
    }

    // Test 8: current schema configs using the old far HUD default still
    // normalize to the closer baseline on load.
    {
        nlohmann::json currentConfig = {
            {"version", 5},
            {"hud", {
                {"distance_meters", 1.5f}
            }}
        };

        {
            std::ofstream file(testPath);
            file << currentConfig.dump(4);
        }

        vrperf::Config config;
        config.Load(testPath);

        assert(config.version == 5);
        assert(config.hud.distanceMeters == 1.0f);

        std::cout << "[PASS] current HUD distance normalization passed" << std::endl;
    }

    // Cleanup
    fs::remove(testPath);

    std::cout << "\n=== All config tests passed ===" << std::endl;
    return 0;
}
