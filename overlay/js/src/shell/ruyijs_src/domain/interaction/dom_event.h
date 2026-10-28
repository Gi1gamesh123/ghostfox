#pragma once

#include "domain/interaction/event_phase.h"

#include <cstdint>
#include <string>

namespace ruyijs::domain::interaction {

/// Entity: Base DOM Event.
/// Represents an event that can be dispatched through the DOM tree.
/// Domain-level abstraction — no V8 dependency.
class DomEvent {
public:
    explicit DomEvent(EventType type, bool bubbles = false, bool cancelable = false)
        : type_(std::move(type)), bubbles_(bubbles), cancelable_(cancelable) {}

    virtual ~DomEvent() = default;

    // --- Immutable properties ---
    const EventType& type() const noexcept { return type_; }
    bool bubbles() const noexcept { return bubbles_; }
    bool cancelable() const noexcept { return cancelable_; }
    bool composed() const noexcept { return composed_; }

    /// isTrusted: true only for events dispatched by the runtime (input simulation)
    bool is_trusted() const noexcept { return is_trusted_; }

    // --- Mutable state (changes during dispatch) ---
    EventPhase event_phase() const noexcept { return phase_; }
    void set_event_phase(EventPhase phase) noexcept { phase_ = phase; }

    bool default_prevented() const noexcept { return default_prevented_; }

    void prevent_default() {
        if (cancelable_ && !passive_) {
            default_prevented_ = true;
        }
    }

    bool propagation_stopped() const noexcept { return stop_propagation_; }
    void stop_propagation() noexcept { stop_propagation_ = true; }

    bool immediate_propagation_stopped() const noexcept { return stop_immediate_; }
    void stop_immediate_propagation() noexcept {
        stop_immediate_ = true;
        stop_propagation_ = true;
    }

    /// Timestamp (performance.now() at creation)
    double time_stamp() const noexcept { return time_stamp_; }
    void set_time_stamp(double ts) noexcept { time_stamp_ = ts; }

    /// Mark this event as trusted (only callable by input simulation service)
    void mark_trusted() noexcept { is_trusted_ = true; }

    void set_composed(bool v) noexcept { composed_ = v; }
    bool dispatching() const noexcept { return dispatching_; }
    bool initialized() const noexcept { return initialized_; }
    void mark_uninitialized() noexcept { initialized_ = false; }
    void set_passive(bool value) noexcept { passive_ = value; }
    uint64_t target_id() const noexcept { return target_id_; }
    void set_target_id(uint64_t value) noexcept { target_id_ = value; }
    uint64_t current_target_id() const noexcept { return current_target_id_; }
    void set_current_target(uint64_t value) noexcept { current_target_id_ = value; }
    void begin_dispatch(uint64_t target) noexcept {
        dispatching_ = true;
        target_id_ = target;
    }
    void end_dispatch() noexcept {
        dispatching_ = passive_ = stop_propagation_ = stop_immediate_ = false;
        current_target_id_ = 0;
        phase_ = EventPhase::None;
    }
    void initialize(EventType type, bool bubbles, bool cancelable) {
        if (dispatching_) return;
        initialized_ = true;
        type_ = std::move(type);
        bubbles_ = bubbles;
        cancelable_ = cancelable;
        default_prevented_ = is_trusted_ = false;
        target_id_ = 0;
        end_dispatch();
    }
    // Public legacy UI/input/storage initialization also mutates an event in
    // flight. Keep the active dispatch scope and passive-listener protection.
    void initialize_legacy(EventType type, bool bubbles, bool cancelable) {
        initialized_ = true;
        type_ = std::move(type);
        bubbles_ = bubbles;
        cancelable_ = cancelable;
        default_prevented_ = is_trusted_ = false;
        stop_propagation_ = stop_immediate_ = false;
        target_id_ = 0;
        if (!dispatching_) end_dispatch();
    }

protected:
    DomEvent(const DomEvent&) = default;
    DomEvent& operator=(const DomEvent&) = default;

private:
    EventType type_;
    bool bubbles_;
    bool cancelable_;
    bool composed_ = false;
    bool is_trusted_ = false;
    EventPhase phase_ = EventPhase::None;
    bool default_prevented_ = false;
    bool stop_propagation_ = false;
    bool stop_immediate_ = false;
    double time_stamp_ = 0.0;
    bool passive_ = false;
    bool dispatching_ = false;
    bool initialized_ = true;
    uint64_t target_id_ = 0;
    uint64_t current_target_id_ = 0;
};

/// MouseEvent domain representation
class MouseEventDomain : public DomEvent {
public:
    MouseEventDomain(EventType type, int client_x, int client_y,
                     int screen_x, int screen_y,
                     int button, int buttons, bool ctrl, bool shift, bool alt, bool meta)
        : DomEvent(std::move(type), /*bubbles=*/true, /*cancelable=*/true),
          client_x_(client_x), client_y_(client_y),
          screen_x_(screen_x), screen_y_(screen_y),
          button_(button), buttons_(buttons),
          ctrl_key_(ctrl), shift_key_(shift), alt_key_(alt), meta_key_(meta) {}

    int client_x() const noexcept { return client_x_; }
    int client_y() const noexcept { return client_y_; }
    int screen_x() const noexcept { return screen_x_; }
    int screen_y() const noexcept { return screen_y_; }
    int button() const noexcept { return button_; }
    int buttons() const noexcept { return buttons_; }
    bool ctrl_key() const noexcept { return ctrl_key_; }
    bool shift_key() const noexcept { return shift_key_; }
    bool alt_key() const noexcept { return alt_key_; }
    bool meta_key() const noexcept { return meta_key_; }

private:
    int client_x_, client_y_;
    int screen_x_, screen_y_;
    int button_, buttons_;
    bool ctrl_key_, shift_key_, alt_key_, meta_key_;
};

/// KeyboardEvent domain representation
class KeyboardEventDomain : public DomEvent {
public:
    KeyboardEventDomain(EventType type, std::string key, std::string code,
                        bool ctrl, bool shift, bool alt, bool meta)
        : DomEvent(std::move(type), /*bubbles=*/true, /*cancelable=*/true),
          key_(std::move(key)), code_(std::move(code)),
          ctrl_key_(ctrl), shift_key_(shift), alt_key_(alt), meta_key_(meta) {}

    const std::string& key() const noexcept { return key_; }
    const std::string& code() const noexcept { return code_; }
    bool ctrl_key() const noexcept { return ctrl_key_; }
    bool shift_key() const noexcept { return shift_key_; }
    bool alt_key() const noexcept { return alt_key_; }
    bool meta_key() const noexcept { return meta_key_; }

private:
    std::string key_, code_;
    bool ctrl_key_, shift_key_, alt_key_, meta_key_;
};

/// UI Events modifier-key mask bits, the values behind getModifierState.
enum ModifierMask : uint32_t {
    kAltMask = 1u << 0, kAltGraphMask = 1u << 1, kCapsLockMask = 1u << 2,
    kControlMask = 1u << 3, kFnMask = 1u << 4, kMetaMask = 1u << 5,
    kNumLockMask = 1u << 6, kScrollLockMask = 1u << 7, kShiftMask = 1u << 8,
    kSymbolMask = 1u << 9, kSymbolLockMask = 1u << 10, kHyperMask = 1u << 11,
    kSuperMask = 1u << 12,
};

/// Modifier-key state shared by the mouse, pointer, keyboard and touch init
/// dictionaries. getModifierState maps the spec key names onto these bits.
struct EventModifiers {
    bool alt_key = false;
    bool ctrl_key = false;
    bool meta_key = false;
    bool shift_key = false;

    uint32_t modifier_mask() const noexcept {
        return (alt_key ? kAltMask : 0u) | (ctrl_key ? kControlMask : 0u) |
               (meta_key ? kMetaMask : 0u) | (shift_key ? kShiftMask : 0u);
    }

    bool modifier(const std::string& key) const noexcept {
        return (key == "Alt" && alt_key) || (key == "Control" && ctrl_key) ||
               (key == "Meta" && meta_key) || (key == "Shift" && shift_key);
    }
};

/// Initialization dictionaries carrying the spec default values of the event
/// subclasses the page installs. JavaScript-value fields (detail, view,
/// relatedTarget, data, source, ports, error, storageArea) stay in the adapter,
/// so the domain owns only the primitive defaults and the modifier semantics.
struct EventInit {
    std::string type;
    bool bubbles = false;
    bool cancelable = false;
    bool composed = false;
};

struct CustomEventInit : EventInit {};

struct UIEventInit : EventInit { int detail = 0; };

struct FocusEventInit : UIEventInit {};

struct MouseEventInit : UIEventInit, EventModifiers {
    double client_x = 0, client_y = 0, screen_x = 0, screen_y = 0;
    int movement_x = 0, movement_y = 0;
    int button = 0, buttons = 0;
};

struct PointerEventInit : MouseEventInit {
    int pointer_id = 0;
    double width = 1.0, height = 1.0;
    double pressure = 0.0, tangential_pressure = 0.0;
    int tilt_x = 0, tilt_y = 0, twist = 0;
    double altitude_angle = 0.0, azimuth_angle = 0.0;
    bool is_primary = false;
    std::string pointer_type;
};

struct KeyboardEventInit : EventInit, EventModifiers {
    std::string key, code;
    int location = 0;
    bool repeat = false, is_composing = false;
};

struct InputEventInit : UIEventInit {
    std::string data, input_type;
    bool is_composing = false;
};

struct WheelEventInit : MouseEventInit {
    double delta_x = 0.0, delta_y = 0.0, delta_z = 0.0;
    int delta_mode = 0;
};

struct TouchEventInit : UIEventInit, EventModifiers {};

struct ProgressEventInit : EventInit {
    bool length_computable = false;
    double loaded = 0.0, total = 0.0;
};

struct MessageEventInit : EventInit { std::string origin, last_event_id; };

struct ErrorEventInit : EventInit {
    std::string message, filename;
    int lineno = 0, colno = 0;
};

struct CloseEventInit : EventInit {
    int code = 0;
    std::string reason;
    bool was_clean = false;
};

struct StorageEventInit : EventInit {
    // key/oldValue/newValue are nullable DOMStrings with a null default.
    bool key_is_null = true, old_value_is_null = true, new_value_is_null = true;
    std::string key, old_value, new_value;
    std::string url;
};

} // namespace ruyijs::domain::interaction
