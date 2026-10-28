#pragma once

#include "domain/browser_environment/navigator_capabilities.h"

#include <string>
#include <vector>
#include <cstdint>

namespace ruyijs::domain::browser_environment {

/// Entity: Navigator information (browser identity and capabilities).
/// Represents the navigator object properties exposed to JavaScript.
class NavigatorInfo {
public:
    NavigatorInfo() = default;

    // --- Identity ---
    const std::string& user_agent() const noexcept { return user_agent_; }
    void set_user_agent(std::string v) { user_agent_ = std::move(v); }

    const std::string& platform() const noexcept { return platform_; }
    void set_platform(std::string v) { platform_ = std::move(v); }

    const std::string& app_name() const noexcept { return app_name_; }
    void set_app_name(std::string v) { app_name_ = std::move(v); }

    const std::string& app_version() const noexcept { return app_version_; }
    void set_app_version(std::string v) { app_version_ = std::move(v); }

    const std::string& app_code_name() const noexcept { return app_code_name_; }
    void set_app_code_name(std::string v) { app_code_name_ = std::move(v); }

    const std::string& product() const noexcept { return product_; }
    void set_product(std::string v) { product_ = std::move(v); }

    const std::string& product_sub() const noexcept { return product_sub_; }
    void set_product_sub(std::string v) { product_sub_ = std::move(v); }

    const std::string& vendor() const noexcept { return vendor_; }
    void set_vendor(std::string v) { vendor_ = std::move(v); }

    const std::string& vendor_sub() const noexcept { return vendor_sub_; }
    void set_vendor_sub(std::string v) { vendor_sub_ = std::move(v); }

    // --- Language ---
    const std::string& language() const noexcept { return language_; }
    void set_language(std::string v) { language_ = std::move(v); }

    const std::vector<std::string>& languages() const noexcept { return languages_; }
    void set_languages(std::vector<std::string> v) { languages_ = std::move(v); }

    // --- Hardware ---
    int hardware_concurrency() const noexcept { return hardware_concurrency_; }
    void set_hardware_concurrency(int v) { hardware_concurrency_ = v; }

    int device_memory() const noexcept { return device_memory_; }
    void set_device_memory(int v) { device_memory_ = v; }

    int max_touch_points() const noexcept { return max_touch_points_; }
    void set_max_touch_points(int v) { max_touch_points_ = v; }

    // --- Capabilities ---
    bool cookie_enabled() const noexcept { return cookie_enabled_; }
    void set_cookie_enabled(bool v) { cookie_enabled_ = v; }

    bool do_not_track() const noexcept { return do_not_track_; }
    void set_do_not_track(bool v) { do_not_track_ = v; }

    bool webdriver() const noexcept { return webdriver_; }
    void set_webdriver(bool v) noexcept { webdriver_ = v; }

    // --- userAgentData ---
    using Brand = UserAgentBrand;

    const std::vector<Brand>& ua_brands() const noexcept { return client_hints_.brands(); }
    void set_ua_brands(std::vector<Brand> v) { client_hints_.set_brands(std::move(v)); }

    bool ua_mobile() const noexcept { return client_hints_.mobile(); }
    void set_ua_mobile(bool v) { client_hints_.set_mobile(v); }

    const std::string& ua_platform() const noexcept { return client_hints_.platform(); }
    void set_ua_platform(std::string v) { client_hints_.set_platform(std::move(v)); }

    const UserAgentClientHints& client_hints() const noexcept { return client_hints_; }
    void set_client_hints(UserAgentClientHints value) { client_hints_ = std::move(value); }
    const NetworkInformation& connection() const noexcept { return connection_; }
    void set_connection(NetworkInformation value) { connection_ = std::move(value); }
    const StorageManagerInfo& storage_manager() const noexcept { return storage_manager_; }
    void set_storage_manager(StorageManagerInfo value) { storage_manager_ = value; }
    const KeyboardLayout& keyboard_layout() const noexcept { return keyboard_layout_; }
    void set_keyboard_layout(KeyboardLayout value) { keyboard_layout_ = std::move(value); }

    /// Apply Chrome/Windows defaults
    void apply_defaults() {
        user_agent_ = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                      "(KHTML, like Gecko) Chrome/125.0.0.0 Safari/537.36";
        platform_ = "Win32";
        app_name_ = "Netscape";
        app_version_ = "5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                       "(KHTML, like Gecko) Chrome/125.0.0.0 Safari/537.36";
        app_code_name_ = "Mozilla";
        product_ = "Gecko";
        product_sub_ = "20030107";
        vendor_ = "Google Inc.";
        vendor_sub_ = "";
        language_ = "en-US";
        languages_ = {"en-US", "en"};
        hardware_concurrency_ = 8;
        device_memory_ = 8;
        max_touch_points_ = 0;
        cookie_enabled_ = true;
        do_not_track_ = false;
        webdriver_ = false;
        client_hints_ = UserAgentClientHints{};
        connection_ = NetworkInformation{};
        storage_manager_ = StorageManagerInfo{};
        keyboard_layout_ = KeyboardLayout{};
    }

private:
    std::string user_agent_;
    std::string platform_;
    std::string app_name_;
    std::string app_version_;
    std::string app_code_name_;
    std::string product_;
    std::string product_sub_;
    std::string vendor_;
    std::string vendor_sub_;
    std::string language_;
    std::vector<std::string> languages_;
    int hardware_concurrency_ = 8;
    int device_memory_ = 8;
    int max_touch_points_ = 0;
    bool cookie_enabled_ = true;
    bool do_not_track_ = false;
    bool webdriver_ = false;
    UserAgentClientHints client_hints_;
    NetworkInformation connection_;
    StorageManagerInfo storage_manager_;
    KeyboardLayout keyboard_layout_;
};

} // namespace ruyijs::domain::browser_environment
