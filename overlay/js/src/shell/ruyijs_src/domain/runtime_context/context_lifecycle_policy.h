#pragma once

#include <string>

namespace ruyijs::domain::runtime_context {

/// Domain Policy: Garbage collection strategy when closing a context.
/// Determines how aggressively V8 should reclaim memory on context disposal.
enum class GcPolicy {
    None,        // No explicit GC (default, fastest close)
    LowMemory,   // Trigger V8 low-memory notification (alias: "v8", true)
    Aggressive,  // Force full GC + idle notification (slowest, most memory freed)
};

/// Domain Service: Context lifecycle policy rules.
/// Encapsulates the decision logic for context creation/destruction.
class ContextLifecyclePolicy {
public:
    /// Parse a GC policy from string representation.
    /// Accepts: "none", "low_memory", "v8", "aggressive", or boolean-like "true"
    static GcPolicy parse_gc_policy(const std::string& gc_str) {
        if (gc_str == "none" || gc_str == "false" || gc_str.empty()) {
            return GcPolicy::None;
        }
        if (gc_str == "low_memory" || gc_str == "v8" || gc_str == "true") {
            return GcPolicy::LowMemory;
        }
        if (gc_str == "aggressive") {
            return GcPolicy::Aggressive;
        }
        return GcPolicy::None;  // Default fallback
    }

    /// Whether the context should perform a heap snapshot before disposal
    static bool should_snapshot_before_dispose(GcPolicy policy) {
        return policy == GcPolicy::Aggressive;
    }

    /// Whether to send idle notification after GC
    static bool should_notify_idle(GcPolicy policy) {
        return policy == GcPolicy::Aggressive;
    }

    /// Maximum number of contexts allowed per process (safety limit)
    static constexpr uint32_t MAX_CONTEXTS_PER_PROCESS = 1024;

    /// Whether a new context can be created given current count
    static bool can_create_context(uint32_t current_count) {
        return current_count < MAX_CONTEXTS_PER_PROCESS;
    }
};

} // namespace ruyijs::domain::runtime_context
