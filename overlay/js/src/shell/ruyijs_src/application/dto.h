#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>



namespace ruyijs::application::dto {

struct ResourceDto {
    std::string url;
    std::string body;
    int status = 200;
    std::map<std::string, std::string> headers;
};

struct EnvironmentDto {
    /// Flattened key-value pairs (e.g., "navigator.userAgent" → "...")
    std::map<std::string, std::string> entries;

    /// Helper: set a nested value by dot-path
    void set(const std::string& path, const std::string& value) {
        entries[path] = value;
    }

    bool has(const std::string& path) const {
        return entries.count(path) > 0;
    }

    std::string get(const std::string& path) const {
        auto it = entries.find(path);
        return (it != entries.end()) ? it->second : "";
    }
};

struct ContextConfigDto {
    std::string mode = "prod";           // "prod" | "debug"
    std::string time_mode = "logical";   // "logical" | "system"
    std::string js_api = "__ruyijs__";      // JS tool object mount name
    EnvironmentDto environment;
    std::vector<std::string> ignore_apis;

    /// DevTools config (optional)
    struct DevToolsConfig {
        bool enabled = false;
        uint16_t port = 9229;
        std::vector<std::string> watch_apis;
        bool enable_console = true;
    };
    DevToolsConfig devtools;
};

} // namespace ruyijs::application::dto
