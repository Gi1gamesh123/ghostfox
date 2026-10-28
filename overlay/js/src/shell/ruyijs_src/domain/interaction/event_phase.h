#pragma once

#include <string>
#include <cstdint>

namespace ruyijs::domain::interaction {

/// Value Object: Event phase during propagation.
enum class EventPhase : uint8_t {
    None = 0,
    Capturing = 1,
    AtTarget = 2,
    Bubbling = 3,
};

inline const char* to_string(EventPhase phase) {
    switch (phase) {
        case EventPhase::None: return "NONE";
        case EventPhase::Capturing: return "CAPTURING_PHASE";
        case EventPhase::AtTarget: return "AT_TARGET";
        case EventPhase::Bubbling: return "BUBBLING_PHASE";
    }
    return "UNKNOWN";
}

/// Value Object: Event type identifier (80+ supported types).
/// Stored as string to support custom events.
class EventType {
public:
    EventType() = default;
    explicit EventType(std::string type) : type_(std::move(type)) {}

    const std::string& value() const noexcept { return type_; }
    bool empty() const noexcept { return type_.empty(); }

    bool operator==(const EventType& o) const { return type_ == o.type_; }
    bool operator!=(const EventType& o) const { return type_ != o.type_; }
    bool operator<(const EventType& o) const { return type_ < o.type_; }

    // Common event type constants
    static EventType click() { return EventType("click"); }
    static EventType mousedown() { return EventType("mousedown"); }
    static EventType mouseup() { return EventType("mouseup"); }
    static EventType mousemove() { return EventType("mousemove"); }
    static EventType pointerdown() { return EventType("pointerdown"); }
    static EventType pointerup() { return EventType("pointerup"); }
    static EventType keydown() { return EventType("keydown"); }
    static EventType keyup() { return EventType("keyup"); }
    static EventType load() { return EventType("load"); }
    static EventType dom_content_loaded() { return EventType("DOMContentLoaded"); }
    static EventType error() { return EventType("error"); }
    static EventType message() { return EventType("message"); }
    static EventType focus() { return EventType("focus"); }
    static EventType blur() { return EventType("blur"); }
    static EventType input() { return EventType("input"); }
    static EventType change() { return EventType("change"); }
    static EventType submit() { return EventType("submit"); }
    static EventType scroll() { return EventType("scroll"); }
    static EventType resize() { return EventType("resize"); }
    static EventType touchstart() { return EventType("touchstart"); }
    static EventType touchend() { return EventType("touchend"); }

private:
    std::string type_;
};

} // namespace ruyijs::domain::interaction
