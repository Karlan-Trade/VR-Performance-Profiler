#include "core/config.h"
#include <fstream>
#include <filesystem>
#include <Windows.h>

namespace vrperf {

namespace fs = std::filesystem;

std::string Config::GetDefaultPath()
{
    char appdata[MAX_PATH];
    if (GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH) == 0) {
        return "config.json";
    }
    fs::path dir = fs::path(appdata) / "VRPerfProfiler";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return (dir / "config.json").string();
}

void Config::SetDefaults()
{
    version = 1;
    overlay = OverlayConfig{};
    hud = HudConfig{};
    wrist = WristConfig{};

    metrics = {
        {"cpu_load",   true,  "CPU Load"},
        {"cpu_temp",   true,  "CPU Temp"},
        {"gpu_load",   true,  "GPU Load"},
        {"gpu_temp",   true,  "GPU Temp"},
        {"gpu_clock",  false, "GPU Clock"},
        {"gpu_memory", true,  "VRAM"},
        {"ram_usage",  true,  "RAM"},
        {"gpu_fan",    false, "GPU Fan"},
    };

    appearance = AppearanceConfig{};
    hotkeys = HotkeyConfig{};
    general = GeneralConfig{};
}

void Config::FromJson(const nlohmann::json& j)
{
    // Version
    version = j.value("version", 1);

    // Overlay
    if (j.contains("overlay")) {
        auto& o = j["overlay"];
        overlay.mode             = o.value("mode", overlay.mode);
        overlay.widthMeters      = o.value("width_meters", overlay.widthMeters);
        overlay.alpha            = o.value("alpha", overlay.alpha);
        overlay.updateIntervalMs = o.value("update_interval_ms", overlay.updateIntervalMs);
        overlay.visibleOnStart   = o.value("visible_on_start", overlay.visibleOnStart);
        overlay.autoConnectVr    = o.value("auto_connect_vr", overlay.autoConnectVr);
    }

    // HUD
    if (j.contains("hud")) {
        auto& h = j["hud"];
        hud.yawDegrees     = h.value("yaw_degrees", hud.yawDegrees);
        hud.pitchDegrees   = h.value("pitch_degrees", hud.pitchDegrees);
        hud.distanceMeters = h.value("distance_meters", hud.distanceMeters);
    }

    // Wrist
    if (j.contains("wrist")) {
        auto& w = j["wrist"];
        wrist.hand       = w.value("hand", wrist.hand);
        wrist.widthMeters = w.value("width_meters", wrist.widthMeters);
        wrist.offsetX    = w.value("offset_x", wrist.offsetX);
        wrist.offsetY    = w.value("offset_y", wrist.offsetY);
        wrist.offsetZ    = w.value("offset_z", wrist.offsetZ);
        wrist.tiltX      = w.value("tilt_x", wrist.tiltX);
        wrist.tiltY      = w.value("tilt_y", wrist.tiltY);
        wrist.tiltZ      = w.value("tilt_z", wrist.tiltZ);
    }

    // Metrics
    if (j.contains("metrics") && j["metrics"].is_array()) {
        metrics.clear();
        for (auto& m : j["metrics"]) {
            MetricConfig mc;
            mc.category = m.value("category", "");
            mc.enabled  = m.value("enabled", true);
            mc.label    = m.value("label", mc.category);
            metrics.push_back(mc);
        }
    }

    // Appearance
    if (j.contains("appearance")) {
        auto& a = j["appearance"];
        appearance.theme        = a.value("theme", appearance.theme);
        appearance.fontSize     = a.value("font_size", appearance.fontSize);
        appearance.textureWidth = a.value("texture_width", appearance.textureWidth);
        appearance.textureHeight = a.value("texture_height", appearance.textureHeight);
    }

    // Hotkeys
    if (j.contains("hotkeys")) {
        auto& hk = j["hotkeys"];
        hotkeys.toggleVisibility = hk.value("toggle_visibility", hotkeys.toggleVisibility);
        hotkeys.switchMode       = hk.value("switch_mode", hotkeys.switchMode);
        hotkeys.cycleMetrics     = hk.value("cycle_metrics", hotkeys.cycleMetrics);
    }

    // General
    if (j.contains("general")) {
        auto& g = j["general"];
        general.startMinimized      = g.value("start_minimized", general.startMinimized);
        general.autoStartHwinfoCheck = g.value("auto_start_hwinfo_check", general.autoStartHwinfoCheck);
        general.logLevel            = g.value("log_level", general.logLevel);
    }
}

nlohmann::json Config::ToJson() const
{
    nlohmann::json j;
    j["version"] = version;

    // Overlay
    j["overlay"] = {
        {"mode", overlay.mode},
        {"width_meters", overlay.widthMeters},
        {"alpha", overlay.alpha},
        {"update_interval_ms", overlay.updateIntervalMs},
        {"visible_on_start", overlay.visibleOnStart},
        {"auto_connect_vr", overlay.autoConnectVr}
    };

    // HUD
    j["hud"] = {
        {"yaw_degrees", hud.yawDegrees},
        {"pitch_degrees", hud.pitchDegrees},
        {"distance_meters", hud.distanceMeters}
    };

    // Wrist
    j["wrist"] = {
        {"hand", wrist.hand},
        {"width_meters", wrist.widthMeters},
        {"offset_x", wrist.offsetX},
        {"offset_y", wrist.offsetY},
        {"offset_z", wrist.offsetZ},
        {"tilt_x", wrist.tiltX},
        {"tilt_y", wrist.tiltY},
        {"tilt_z", wrist.tiltZ}
    };

    // Metrics
    j["metrics"] = nlohmann::json::array();
    for (auto& m : metrics) {
        j["metrics"].push_back({
            {"category", m.category},
            {"enabled", m.enabled},
            {"label", m.label}
        });
    }

    // Appearance
    j["appearance"] = {
        {"theme", appearance.theme},
        {"font_size", appearance.fontSize},
        {"texture_width", appearance.textureWidth},
        {"texture_height", appearance.textureHeight}
    };

    // Hotkeys
    j["hotkeys"] = {
        {"toggle_visibility", hotkeys.toggleVisibility},
        {"switch_mode", hotkeys.switchMode},
        {"cycle_metrics", hotkeys.cycleMetrics}
    };

    // General
    j["general"] = {
        {"start_minimized", general.startMinimized},
        {"auto_start_hwinfo_check", general.autoStartHwinfoCheck},
        {"log_level", general.logLevel}
    };

    return j;
}

bool Config::Load(const std::string& path)
{
    std::string configPath = path.empty() ? GetDefaultPath() : path;

    SetDefaults();

    std::ifstream file(configPath);
    if (!file.is_open()) {
        // No config file yet — use defaults and save
        Save(configPath);
        return true;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(file);
        FromJson(j);
    } catch (const std::exception& e) {
        // Malformed config — keep defaults
        SetDefaults();
        return false;
    }

    return true;
}

bool Config::Save(const std::string& path)
{
    std::string configPath = path.empty() ? GetDefaultPath() : path;

    // Ensure directory exists
    fs::path dir = fs::path(configPath).parent_path();
    std::error_code ec;
    fs::create_directories(dir, ec);

    std::ofstream file(configPath);
    if (!file.is_open()) {
        return false;
    }

    file << ToJson().dump(4);
    return true;
}

} // namespace vrperf
