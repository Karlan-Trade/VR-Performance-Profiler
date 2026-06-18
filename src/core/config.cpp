#include "core/config.h"
#include <fstream>
#include <filesystem>
#include <Windows.h>
#include <algorithm>

namespace vrperf {

namespace fs = std::filesystem;

namespace {

std::wstring ToWideForJson(const std::string& text)
{
    if (text.empty()) {
        return {};
    }

    int length = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.c_str(),
        static_cast<int>(text.size()),
        nullptr,
        0);
    UINT codePage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    if (length <= 0) {
        codePage = CP_ACP;
        flags = 0;
        length = MultiByteToWideChar(
            codePage,
            flags,
            text.c_str(),
            static_cast<int>(text.size()),
            nullptr,
            0);
    }
    if (length <= 0) {
        return std::wstring(text.begin(), text.end());
    }

    std::wstring wide(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(
        codePage,
        flags,
        text.c_str(),
        static_cast<int>(text.size()),
        wide.data(),
        length);
    return wide;
}

std::string SafeUtf8ForJson(const std::string& text)
{
    const auto wide = ToWideForJson(text);
    if (wide.empty()) {
        return {};
    }

    int length = WideCharToMultiByte(
        CP_UTF8,
        0,
        wide.c_str(),
        static_cast<int>(wide.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (length <= 0) {
        return {};
    }

    std::string utf8(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        wide.c_str(),
        static_cast<int>(wide.size()),
        utf8.data(),
        length,
        nullptr,
        nullptr);
    return utf8;
}

} // namespace

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
    version = 7;
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
        {"vr_fps",     true,  "VR FPS"},
        {"vr_frame_time", false, "VR Frame"},
        {"vr_gpu_frame_time", false, "VR GPU"},
        {"vr_refresh_rate", false, "VR Refresh"},
        {"vr_dropped_frames", true, "VR Drops"},
    };

    appearance = AppearanceConfig{};
    data = DataConfig{};
    general = GeneralConfig{};
}

void Config::FromJson(const nlohmann::json& j)
{
    // Version
    const int loadedVersion = j.value("version", 1);
    version = loadedVersion;

    // Overlay
    if (j.contains("overlay")) {
        auto& o = j["overlay"];
        overlay.mode             = o.value("mode", overlay.mode);
        overlay.widthMeters      = o.value("width_meters", overlay.widthMeters);
        overlay.alpha            = o.value("alpha", overlay.alpha);
        overlay.updateIntervalMs = o.value("update_interval_ms", overlay.updateIntervalMs);
        overlay.visibleOnStart   = o.value("visible_on_start", overlay.visibleOnStart);
        overlay.autoConnectVr    = o.value("auto_connect_vr", overlay.autoConnectVr);
        overlay.offsetX          = o.value("offset_x", overlay.offsetX);
        overlay.offsetY          = o.value("offset_y", overlay.offsetY);
        overlay.offsetZ          = o.value("offset_z", overlay.offsetZ);
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
        wrist.offsetScale = (std::clamp)(
            w.value("offset_scale", wrist.offsetScale),
            0.01f,
            1.0f);
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
            mc.sensorKey = m.value("sensor_key", "");
            mc.source = m.value("source", "");
            mc.sensorId = m.value("sensor_id", -1);
            mc.readingId = m.value("reading_id", -1);
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

    // Data source
    if (j.contains("data")) {
        auto& d = j["data"];
        data.hardwareSource = d.value("hardware_source", data.hardwareSource);
    }
    if (data.hardwareSource != "hwinfo") {
        data.hardwareSource = "afterburner";
    }

    // General
    if (j.contains("general")) {
        auto& g = j["general"];
        general.startMinimized      = g.value("start_minimized", general.startMinimized);
        general.language            = g.value("language", general.language);
        general.logLevel            = g.value("log_level", general.logLevel);
    }

    if (loadedVersion < 2) {
        overlay.autoConnectVr = false;
        version = 2;
    }

    if (loadedVersion < 3) {
        if (overlay.widthMeters <= 0.31f) {
            overlay.widthMeters = 1.0f;
        }
        if (hud.pitchDegrees == -15.0f) {
            hud.pitchDegrees = 0.0f;
        }
        if (hud.distanceMeters <= 1.01f) {
            hud.distanceMeters = 1.5f;
        }
        version = 3;
    }

    if (loadedVersion < 4) {
        if (overlay.widthMeters <= 1.01f) {
            overlay.widthMeters = 1.5f;
        }
        version = 4;
    }

    if (hud.distanceMeters >= 1.49f && hud.distanceMeters <= 1.51f) {
        hud.distanceMeters = 1.0f;
    }

    if (loadedVersion < 5) {
        version = 5;
    }

    if (loadedVersion < 6) {
        data.hardwareSource = "afterburner";
        version = 6;
    }

    if (loadedVersion < 7) {
        version = 7;
    }

    overlay.autoConnectVr = false;

    if (loadedVersion < 7) {
        wrist.widthMeters = 0.75f;
        wrist.offsetX = 0.0f;
        wrist.offsetY = 0.0f;
        wrist.offsetZ = 0.0f;
        wrist.offsetScale = 0.25f;
        wrist.tiltX = 0.0f;
        wrist.tiltY = 0.0f;
        wrist.tiltZ = 0.0f;
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
        {"auto_connect_vr", overlay.autoConnectVr},
        {"offset_x", overlay.offsetX},
        {"offset_y", overlay.offsetY},
        {"offset_z", overlay.offsetZ}
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
        {"offset_scale", wrist.offsetScale},
        {"tilt_x", wrist.tiltX},
        {"tilt_y", wrist.tiltY},
        {"tilt_z", wrist.tiltZ}
    };

    // Metrics
    j["metrics"] = nlohmann::json::array();
    for (auto& m : metrics) {
        auto metric = nlohmann::json{
            {"category", SafeUtf8ForJson(m.category)},
            {"enabled", m.enabled},
            {"label", SafeUtf8ForJson(m.label)}
        };

        if (!m.sensorKey.empty()) {
            metric["sensor_key"] = SafeUtf8ForJson(m.sensorKey);
        }
        if (!m.source.empty()) {
            metric["source"] = SafeUtf8ForJson(m.source);
        }
        if (m.sensorId >= 0) {
            metric["sensor_id"] = m.sensorId;
        }
        if (m.readingId >= 0) {
            metric["reading_id"] = m.readingId;
        }

        j["metrics"].push_back(metric);
    }

    // Appearance
    j["appearance"] = {
        {"theme", appearance.theme},
        {"font_size", appearance.fontSize},
        {"texture_width", appearance.textureWidth},
        {"texture_height", appearance.textureHeight}
    };

    // Data source
    j["data"] = {
        {"hardware_source", data.hardwareSource == "hwinfo" ? "hwinfo" : "afterburner"}
    };

    // General
    j["general"] = {
        {"start_minimized", general.startMinimized},
        {"language", general.language},
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
        // No config file yet - use defaults and save.
        Save(configPath);
        return true;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(file);
        FromJson(j);
    } catch (const std::exception& e) {
        // Malformed config - keep defaults.
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

    file << ToJson().dump(
        4,
        ' ',
        false,
        nlohmann::json::error_handler_t::replace);
    return true;
}

} // namespace vrperf
