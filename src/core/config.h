#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace vrperf {

struct MetricConfig {
    std::string category;
    bool enabled = true;
    std::string label;
    std::string sensorKey;
    std::string source;
    int sensorId = -1;
    int readingId = -1;
};

struct OverlayConfig {
    std::string mode = "hud";        // "hud" or "wrist"
    float widthMeters = 1.5f;
    float alpha = 0.85f;
    int updateIntervalMs = 66;        // ~15fps
    bool visibleOnStart = true;
    bool autoConnectVr = false;
};

struct HudConfig {
    float yawDegrees = 0.0f;
    float pitchDegrees = 0.0f;
    float distanceMeters = 1.0f;
};

struct WristConfig {
    std::string hand = "left";
    float widthMeters = 0.75f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float offsetZ = 0.0f;
    float tiltX = 0.0f;
    float tiltY = 0.0f;
    float tiltZ = 0.0f;
};

struct AppearanceConfig {
    std::string theme = "dark";
    int fontSize = 28;
    int textureWidth = 1024;
    int textureHeight = 512;
};

struct HotkeyConfig {
    std::string toggleVisibility = "ctrl+shift+h";
    std::string switchMode = "ctrl+shift+m";
    std::string cycleMetrics = "ctrl+shift+n";
};

struct DataConfig {
    std::string hardwareSource = "afterburner"; // "afterburner" or "hwinfo"
};

struct GeneralConfig {
    bool startMinimized = true;
    std::string language = "zh";
    std::string logLevel = "info";
};

class Config {
public:
    // Public fields for direct access
    int version = 1;
    OverlayConfig overlay;
    HudConfig hud;
    WristConfig wrist;
    std::vector<MetricConfig> metrics;
    AppearanceConfig appearance;
    HotkeyConfig hotkeys;
    DataConfig data;
    GeneralConfig general;

    // Load config from default path or specified path
    bool Load(const std::string& path = "");
    bool Save(const std::string& path = "");

    // Get default config path (%APPDATA%/VRPerfProfiler/config.json)
    static std::string GetDefaultPath();

private:
    void SetDefaults();
    void FromJson(const nlohmann::json& j);
    nlohmann::json ToJson() const;
};

} // namespace vrperf
