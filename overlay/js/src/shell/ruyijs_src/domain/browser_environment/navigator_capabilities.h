#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace ruyijs::domain::browser_environment {

struct UserAgentBrand {
    std::string brand;
    std::string version;
};

class UserAgentClientHints {
public:
    const std::vector<UserAgentBrand>& brands() const noexcept { return brands_; }
    void set_brands(std::vector<UserAgentBrand> value) { brands_ = std::move(value); }

    const std::vector<UserAgentBrand>& full_version_list() const noexcept { return full_version_list_; }
    void set_full_version_list(std::vector<UserAgentBrand> value) { full_version_list_ = std::move(value); }

    const std::string& platform() const noexcept { return platform_; }
    void set_platform(std::string value) { platform_ = std::move(value); }
    const std::string& platform_version() const noexcept { return platform_version_; }
    void set_platform_version(std::string value) { platform_version_ = std::move(value); }
    const std::string& architecture() const noexcept { return architecture_; }
    void set_architecture(std::string value) { architecture_ = std::move(value); }
    const std::string& bitness() const noexcept { return bitness_; }
    void set_bitness(std::string value) { bitness_ = std::move(value); }
    const std::string& model() const noexcept { return model_; }
    void set_model(std::string value) { model_ = std::move(value); }
    const std::string& full_version() const noexcept { return full_version_; }
    void set_full_version(std::string value) { full_version_ = std::move(value); }
    bool mobile() const noexcept { return mobile_; }
    void set_mobile(bool value) noexcept { mobile_ = value; }
    bool wow64() const noexcept { return wow64_; }
    void set_wow64(bool value) noexcept { wow64_ = value; }

private:
    std::vector<UserAgentBrand> brands_ = {
        {"Not;A=Brand", "8"}, {"Chromium", "125"}, {"Google Chrome", "125"},
    };
    std::vector<UserAgentBrand> full_version_list_ = {
        {"Not;A=Brand", "8.0.0.0"},
        {"Chromium", "125.0.6422.112"},
        {"Google Chrome", "125.0.6422.112"},
    };
    std::string platform_ = "Windows";
    std::string platform_version_ = "15.0.0";
    std::string architecture_ = "x86";
    std::string bitness_ = "64";
    std::string model_;
    std::string full_version_ = "125.0.6422.112";
    bool mobile_ = false;
    bool wow64_ = false;
};

class NetworkInformation {
public:
    const std::string& effective_type() const noexcept { return effective_type_; }
    void set_effective_type(std::string value) { effective_type_ = std::move(value); }
    double downlink() const noexcept { return downlink_; }
    void set_downlink(double value) noexcept { downlink_ = value; }
    double rtt() const noexcept { return rtt_; }
    void set_rtt(double value) noexcept { rtt_ = value; }
    bool save_data() const noexcept { return save_data_; }
    void set_save_data(bool value) noexcept { save_data_ = value; }
    const std::string& type() const noexcept { return type_; }
    void set_type(std::string value) { type_ = std::move(value); type_is_null_ = false; }
    bool type_is_null() const noexcept { return type_is_null_; }
    void set_type_null() noexcept { type_is_null_ = true; }
    double downlink_max() const noexcept { return downlink_max_; }
    void set_downlink_max(double value) noexcept { downlink_max_ = value; downlink_max_is_null_ = false; }
    bool downlink_max_is_null() const noexcept { return downlink_max_is_null_; }
    void set_downlink_max_null() noexcept { downlink_max_is_null_ = true; }

private:
    std::string effective_type_ = "4g";
    double downlink_ = 10.0;
    double rtt_ = 50.0;
    bool save_data_ = false;
    std::string type_ = "wifi";
    double downlink_max_ = 100.0;
    bool type_is_null_ = false;
    bool downlink_max_is_null_ = false;
};

class StorageManagerInfo {
public:
    uint64_t quota() const noexcept { return quota_; }
    void set_quota(uint64_t value) noexcept { quota_ = value; }
    uint64_t usage() const noexcept { return usage_; }
    void set_usage(uint64_t value) noexcept { usage_ = value; }
    bool persisted() const noexcept { return persisted_; }
    void set_persisted(bool value) noexcept { persisted_ = value; }

private:
    uint64_t quota_ = 1073741824ULL;
    uint64_t usage_ = 0;
    bool persisted_ = false;
};

struct KeyboardLayoutEntry {
    std::string code;
    std::string value;
};

class KeyboardLayout {
public:
    const std::vector<KeyboardLayoutEntry>& entries() const noexcept { return entries_; }
    void set_entries(std::vector<KeyboardLayoutEntry> value) { entries_ = std::move(value); }

private:
    std::vector<KeyboardLayoutEntry> entries_;
};

} // namespace ruyijs::domain::browser_environment
