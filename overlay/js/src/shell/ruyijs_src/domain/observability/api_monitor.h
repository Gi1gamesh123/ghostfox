#pragma once

#include "domain/shared/aggregate_root.h"
#include "domain/shared/domain_event.h"

#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>

namespace ruyijs::domain::observability {

using ApiMonitorId = uint64_t;

/// Value Object: API access path (e.g., "navigator.userAgent", "document.cookie")
class ApiPath {
public:
    ApiPath() = default;
    explicit ApiPath(std::string path) : path_(std::move(path)) {}

    const std::string& value() const noexcept { return path_; }
    bool empty() const noexcept { return path_.empty(); }

    bool operator==(const ApiPath& o) const { return path_ == o.path_; }
    bool operator<(const ApiPath& o) const { return path_ < o.path_; }

private:
    std::string path_;
};

/// Value Object: Type of API access.
enum class CallType {
    PropertyRead,
    PropertyWrite,
    MethodCall,
    Construct,
};

inline const char* call_type_name(CallType t) {
    switch (t) {
        case CallType::PropertyRead: return "get";
        case CallType::PropertyWrite: return "set";
        case CallType::MethodCall: return "call";
        case CallType::Construct: return "construct";
    }
    return "unknown";
}

/// Value Object: Monitor policy (which APIs to ignore/silence).
class MonitorPolicy {
public:
    MonitorPolicy() {
        // Default silenced APIs (high-frequency built-ins)
        ignored_prefixes_ = {
            "Math.", "JSON.", "Array.", "Int8Array.", "Uint8Array.",
            "Int16Array.", "Uint16Array.", "Int32Array.", "Uint32Array.",
            "Float32Array.", "Float64Array.", "BigInt64Array.", "BigUint64Array.",
        };
    }

    /// Check if an API path should be silenced
    bool is_ignored(const ApiPath& path) const {
        const std::string& p = path.value();
        for (const auto& prefix : ignored_prefixes_) {
            if (p.rfind(prefix, 0) == 0) return true;
        }
        // Also check exact matches
        return std::find(ignored_exact_.begin(), ignored_exact_.end(), p) != ignored_exact_.end();
    }

    void add_ignored_exact(const std::string& path) {
        ignored_exact_.push_back(path);
    }

    void add_ignored_prefix(const std::string& prefix) {
        ignored_prefixes_.push_back(prefix);
    }

    const std::vector<std::string>& ignored_prefixes() const noexcept { return ignored_prefixes_; }
    const std::vector<std::string>& ignored_exact() const noexcept { return ignored_exact_; }

private:
    std::vector<std::string> ignored_prefixes_;
    std::vector<std::string> ignored_exact_;
};

/// Entity: A single API call record.
class ApiCallRecord {
public:
    ApiCallRecord(ApiPath path, CallType type, std::string detail = "")
        : path_(std::move(path)), type_(type), detail_(std::move(detail)),
          sequence_(next_seq()) {}

    const ApiPath& path() const noexcept { return path_; }
    CallType type() const noexcept { return type_; }
    const std::string& detail() const noexcept { return detail_; }
    uint64_t sequence() const noexcept { return sequence_; }

    /// Format as human-readable string
    std::string to_string() const {
        std::string result = call_type_name(type_);
        result += " " + path_.value();
        if (!detail_.empty()) result += " (" + detail_ + ")";
        return result;
    }

private:
    static uint64_t next_seq() {
        static uint64_t counter = 0;
        return ++counter;
    }

    ApiPath path_;
    CallType type_;
    std::string detail_;
    uint64_t sequence_;
};

/// Port (Interface): Debugger (DevTools CDP).
/// Domain defines the interface; infrastructure provides WebSocket CDP server.
class DebuggerPort {
public:
    virtual ~DebuggerPort() = default;

    virtual void start(uint16_t port) = 0;
    virtual void stop() = 0;
    virtual bool is_running() const = 0;

    /// Pause execution (breakpoint hit)
    virtual void pause() = 0;
    virtual void resume() = 0;

    /// API access breakpoint
    virtual void add_api_breakpoint(const std::string& api_path) = 0;
    virtual void remove_api_breakpoint(const std::string& api_path) = 0;
};

/// Domain Service: Native function disguise.
/// Makes JS functions appear as [native code] via Function.prototype.toString interception.
class NativeDisguiseService {
public:
    /// Generate the disguised toString output for a function
    static std::string disguised_to_string(const std::string& function_name) {
        return "function " + function_name + "() { [native code] }";
    }

    /// Check if a toString output looks native
    static bool looks_native(const std::string& to_string_output) {
        return to_string_output.find("[native code]") != std::string::npos;
    }

    /// Validate a function name for disguise (must be a valid JS identifier)
    static bool is_valid_name(const std::string& name) {
        if (name.empty()) return false;
        if (!std::isalpha(name[0]) && name[0] != '_' && name[0] != '$') return false;
        for (size_t i = 1; i < name.size(); ++i) {
            char c = name[i];
            if (!std::isalnum(c) && c != '_' && c != '$') return false;
        }
        return true;
    }
};

/// Aggregate Root: ApiMonitor
/// Tracks browser API access in debug mode for environment detection analysis.
class ApiMonitorAggregate : public shared::AggregateRoot<ApiMonitorId> {
public:
    explicit ApiMonitorAggregate(ApiMonitorId id = 1)
        : AggregateRoot<ApiMonitorId>(id) {}

    bool enabled() const noexcept { return enabled_; }
    void enable() noexcept { enabled_ = true; }
    void disable() noexcept { enabled_ = false; }

    /// Record an API access (respects ignore policy)
    void record_access(const ApiPath& path, CallType type, const std::string& detail = "") {
        if (!enabled_) return;
        if (policy_.is_ignored(path)) return;
        records_.emplace_back(path, type, detail);
    }

    const std::vector<ApiCallRecord>& records() const noexcept { return records_; }
    size_t record_count() const noexcept { return records_.size(); }

    void clear_records() { records_.clear(); }

    /// Get all records for a specific API path
    std::vector<const ApiCallRecord*> records_for(const ApiPath& path) const {
        std::vector<const ApiCallRecord*> result;
        for (const auto& record : records_) {
            if (record.path() == path) result.push_back(&record);
        }
        return result;
    }

    // --- Policy ---
    MonitorPolicy& policy() noexcept { return policy_; }
    const MonitorPolicy& policy() const noexcept { return policy_; }

    // --- Watch APIs (breakpoints) ---
    void add_watch(const std::string& api_path) {
        watch_apis_.push_back(api_path);
    }

    bool is_watched(const ApiPath& path) const {
        return std::find(watch_apis_.begin(), watch_apis_.end(), path.value()) != watch_apis_.end();
    }

    const std::vector<std::string>& watch_apis() const noexcept { return watch_apis_; }

private:
    bool enabled_ = false;
    MonitorPolicy policy_;
    std::vector<ApiCallRecord> records_;
    std::vector<std::string> watch_apis_;
};

// --- Domain Events ---

/// Raised when a monitored API is accessed.
class ApiAccessedEvent : public shared::DomainEvent {
public:
    ApiAccessedEvent(std::string path, CallType type)
        : DomainEvent("ApiAccessed"), path_(std::move(path)), type_(type) {}
    const std::string& path() const noexcept { return path_; }
    CallType type() const noexcept { return type_; }
private:
    std::string path_;
    CallType type_;
};

/// Raised when a watch_apis breakpoint is hit.
class BreakpointHitEvent : public shared::DomainEvent {
public:
    explicit BreakpointHitEvent(std::string api_path)
        : DomainEvent("BreakpointHit"), api_path_(std::move(api_path)) {}
    const std::string& api_path() const noexcept { return api_path_; }
private:
    std::string api_path_;
};

} // namespace ruyijs::domain::observability
