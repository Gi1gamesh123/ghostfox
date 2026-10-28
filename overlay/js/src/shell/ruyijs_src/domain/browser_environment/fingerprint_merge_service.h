#pragma once

#include "domain/browser_environment/fingerprint_config.h"
#include "domain/browser_environment/navigator_info.h"
#include "domain/browser_environment/screen_info.h"
#include "domain/browser_environment/location_info.h"

namespace ruyijs::domain::browser_environment {

/// Domain Service: Fingerprint configuration merge logic.
/// Responsible for:
///   1. Building the default fingerprint (200+ fields, Chrome/Windows baseline)
///   2. Deep-merging user-provided environment overrides onto defaults
///   3. Applying merged config to Navigator/Screen/Location entities
class FingerprintMergeService {
public:
    /// Build the complete default fingerprint configuration
    static FingerprintConfig build_defaults() {
        FingerprintConfig config;

        // Navigator defaults
        config.set("navigator.userAgent",
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
            "(KHTML, like Gecko) Chrome/125.0.0.0 Safari/537.36");
        config.set("navigator.platform", "Win32");
        config.set("navigator.language", "en-US");
        config.set("navigator.languages", "en-US,en");
        config.set("navigator.hardwareConcurrency", "8");
        config.set("navigator.deviceMemory", "8");
        config.set("navigator.maxTouchPoints", "0");
        config.set("navigator.cookieEnabled", "true");
        config.set("navigator.doNotTrack", "false");
        config.set("navigator.webdriver", "false");
        config.set("navigator.vendor", "Google Inc.");
        config.set("navigator.product", "Gecko");
        config.set("navigator.productSub", "20030107");
        config.set("navigator.appName", "Netscape");
        config.set("navigator.appCodeName", "Mozilla");

        // Screen defaults
        config.set("screen.width", "1920");
        config.set("screen.height", "1080");
        config.set("screen.availWidth", "1920");
        config.set("screen.availHeight", "1040");
        config.set("screen.colorDepth", "24");
        config.set("screen.pixelDepth", "24");

        // Window defaults
        config.set("window.devicePixelRatio", "1.0");
        config.set("window.innerWidth", "1920");
        config.set("window.innerHeight", "937");
        config.set("window.outerWidth", "1920");
        config.set("window.outerHeight", "1040");
        config.set("window.screenX", "0");
        config.set("window.screenY", "0");

        // WebGL defaults
        config.set("webgl.vendor", "Google Inc. (NVIDIA)");
        config.set("webgl.renderer", "ANGLE (NVIDIA, NVIDIA GeForce GTX 1080 Direct3D11 vs_5_0 ps_5_0)");
        config.set("webgl.version", "WebGL 1.0 (OpenGL ES 2.0 Chromium)");
        config.set("webgl.shadingLanguageVersion", "WebGL GLSL ES 1.0 (OpenGL ES GLSL ES 1.0 Chromium)");

        // Location defaults
        config.set("location.href", "https://example.com/");
        config.set("location.protocol", "https:");
        config.set("location.hostname", "example.com");
        config.set("location.pathname", "/");

        return config;
    }

    /// Merge user overrides onto defaults.
    /// User values always win on conflict. Unspecified fields keep defaults.
    static FingerprintConfig merge(const FingerprintConfig& defaults,
                                   const FingerprintConfig& overrides) {
        FingerprintConfig result = defaults;
        result.merge_from(overrides);
        return result;
    }

    /// Apply merged configuration to a NavigatorInfo entity
    static void apply_to_navigator(const FingerprintConfig& config, NavigatorInfo& nav) {
        nav.apply_defaults();  // Start from defaults

        if (config.has("navigator.userAgent"))
            nav.set_user_agent(config.get("navigator.userAgent"));
        if (config.has("navigator.platform"))
            nav.set_platform(config.get("navigator.platform"));
        if (config.has("navigator.language"))
            nav.set_language(config.get("navigator.language"));
        if (config.has("navigator.hardwareConcurrency"))
            nav.set_hardware_concurrency(std::stoi(config.get("navigator.hardwareConcurrency")));
        if (config.has("navigator.deviceMemory"))
            nav.set_device_memory(std::stoi(config.get("navigator.deviceMemory")));
        if (config.has("navigator.maxTouchPoints"))
            nav.set_max_touch_points(std::stoi(config.get("navigator.maxTouchPoints")));
        if (config.has("navigator.vendor"))
            nav.set_vendor(config.get("navigator.vendor"));
    }

    /// Apply merged configuration to a ScreenInfo entity
    static void apply_to_screen(const FingerprintConfig& config, ScreenInfo& screen) {
        screen.apply_defaults();

        if (config.has("screen.width"))
            screen.set_width(std::stoi(config.get("screen.width")));
        if (config.has("screen.height"))
            screen.set_height(std::stoi(config.get("screen.height")));
        if (config.has("screen.availWidth"))
            screen.set_avail_width(std::stoi(config.get("screen.availWidth")));
        if (config.has("screen.availHeight"))
            screen.set_avail_height(std::stoi(config.get("screen.availHeight")));
        if (config.has("screen.colorDepth"))
            screen.set_color_depth(std::stoi(config.get("screen.colorDepth")));
        if (config.has("screen.pixelDepth"))
            screen.set_pixel_depth(std::stoi(config.get("screen.pixelDepth")));
    }

    /// Apply merged configuration to a LocationInfo entity
    static void apply_to_location(const FingerprintConfig& config, LocationInfo& loc) {
        if (config.has("location.href")) {
            loc.navigate(config.get("location.href"));
        } else {
            loc.apply_defaults();
        }
    }
};

} // namespace ruyijs::domain::browser_environment
