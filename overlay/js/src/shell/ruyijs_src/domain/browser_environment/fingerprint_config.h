#pragma once

#include <any>
#include <map>
#include <string>
#include <vector>

namespace ruyijs::domain::browser_environment {

/// Value Object: A single fingerprint configuration entry.
/// Represents a dot-path key (e.g., "navigator.userAgent") and its value.
struct FingerprintEntry {
    std::string path;   // Dot-separated path, e.g. "navigator.userAgent"
    std::string value;  // String representation of the value

    bool operator==(const FingerprintEntry& o) const {
        return path == o.path && value == o.value;
    }
};

/// Value Object: Complete fingerprint configuration.
/// Holds 200+ configurable fields organized by dot-path.
/// The infrastructure layer converts these to actual V8 object properties.
class FingerprintConfig {
public:
    FingerprintConfig() = default;

    /// Set a configuration value by dot-path
    void set(const std::string& path, const std::string& value) {
        entries_[path] = value;
    }

    /// Get a configuration value (returns empty string if not found)
    std::string get(const std::string& path) const {
        auto it = entries_.find(path);
        return (it != entries_.end()) ? it->second : "";
    }

    /// Check if a path is configured
    bool has(const std::string& path) const {
        return entries_.count(path) > 0;
    }

    /// Get all entries
    const std::map<std::string, std::string>& entries() const noexcept {
        return entries_;
    }

    /// Get all paths under a prefix (e.g., "navigator." returns all navigator fields)
    std::vector<std::string> paths_under(const std::string& prefix) const {
        std::vector<std::string> result;
        for (const auto& [path, _] : entries_) {
            if (path.rfind(prefix, 0) == 0) {
                result.push_back(path);
            }
        }
        return result;
    }

    size_t size() const noexcept { return entries_.size(); }
    bool empty() const noexcept { return entries_.empty(); }

    /// Merge another config on top (other's values win on conflict)
    void merge_from(const FingerprintConfig& other) {
        for (const auto& [path, value] : other.entries_) {
            entries_[path] = value;
        }
    }

private:
    std::map<std::string, std::string> entries_;
};

} // namespace ruyijs::domain::browser_environment
