#pragma once

#include <cstdint>
#include <string>

namespace ruyijs::domain::execution_scheduling {

/// Value Object: Virtual time point (milliseconds since context creation).
class TimePoint {
public:
    TimePoint() = default;
    explicit TimePoint(double ms) : ms_(ms) {}

    double ms() const noexcept { return ms_; }

    TimePoint operator+(double delta) const { return TimePoint(ms_ + delta); }
    TimePoint operator-(double delta) const { return TimePoint(ms_ - delta); }
    double operator-(const TimePoint& other) const { return ms_ - other.ms_; }

    bool operator<(const TimePoint& o) const { return ms_ < o.ms_; }
    bool operator<=(const TimePoint& o) const { return ms_ <= o.ms_; }
    bool operator>(const TimePoint& o) const { return ms_ > o.ms_; }
    bool operator>=(const TimePoint& o) const { return ms_ >= o.ms_; }
    bool operator==(const TimePoint& o) const { return ms_ == o.ms_; }
    bool operator!=(const TimePoint& o) const { return ms_ != o.ms_; }

private:
    double ms_ = 0.0;
};

/// Value Object: Duration in milliseconds.
class Duration {
public:
    Duration() = default;
    explicit Duration(double ms) : ms_(ms < 0 ? 0 : ms) {}

    double ms() const noexcept { return ms_; }
    bool is_zero() const noexcept { return ms_ == 0.0; }

    Duration operator+(const Duration& o) const { return Duration(ms_ + o.ms_); }
    bool operator<(const Duration& o) const { return ms_ < o.ms_; }

    static Duration from_seconds(double s) { return Duration(s * 1000.0); }
    static constexpr double FRAME_MS = 16.667;  // ~60fps frame duration

private:
    double ms_ = 0.0;
};

/// Value Object: Time mode for the event loop.
enum class TimeMode {
    Logical,  // Pure logical time advancement (sleep(5000) completes instantly)
    System,   // System clock anchored (Date.now() reflects real elapsed time)
};

/// Value Object: Task priority levels for macro-task scheduling.
/// Lower numeric value = higher priority (executed first within same deadline).
enum class TaskPriority : uint8_t {
    Timer = 0,          // setTimeout / setInterval callbacks
    Animation = 1,      // requestAnimationFrame callbacks
    Idle = 2,           // requestIdleCallback
    Network = 3,        // XHR / fetch callbacks
    Observer = 4,       // MutationObserver / IntersectionObserver
    UserDefined = 5,    // queueMicrotask (handled separately as microtask)
};

inline const char* priority_name(TaskPriority p) {
    switch (p) {
        case TaskPriority::Timer: return "timer";
        case TaskPriority::Animation: return "animation";
        case TaskPriority::Idle: return "idle";
        case TaskPriority::Network: return "network";
        case TaskPriority::Observer: return "observer";
        case TaskPriority::UserDefined: return "user";
    }
    return "unknown";
}

} // namespace ruyijs::domain::execution_scheduling
