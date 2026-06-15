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
        assert(config.overlay.widthMeters == 0.3f);
        assert(config.overlay.alpha == 0.85f);
        assert(config.overlay.autoConnectVr == false);
        assert(config.hud.pitchDegrees == -15.0f);
        assert(config.wrist.hand == "left");
        assert(config.metrics.size() == 8);
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
        config.overlay.alpha = 0.9f;
        config.overlay.updateIntervalMs = 1000;
        config.appearance.theme = "light";
        config.general.language = "en";

        // Disable a metric
        for (auto& m : config.metrics) {
            if (m.category == "gpu_clock") {
                m.enabled = true;
            }
        }

        assert(config.Save(testPath));

        // Reload and verify
        vrperf::Config config2;
        config2.Load(testPath);

        assert(config2.overlay.mode == "wrist");
        assert(config2.overlay.alpha == 0.9f);
        assert(config2.overlay.updateIntervalMs == 1000);
        assert(config2.appearance.theme == "light");
        assert(config2.general.language == "en");

        for (const auto& m : config2.metrics) {
            if (m.category == "gpu_clock") {
                assert(m.enabled == true);
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

        assert(j["metrics"].is_array());
        assert(j["metrics"].size() == 8);

        std::cout << "[PASS] JSON structure valid" << std::endl;
    }

    // Cleanup
    fs::remove(testPath);

    std::cout << "\n=== All config tests passed ===" << std::endl;
    return 0;
}
