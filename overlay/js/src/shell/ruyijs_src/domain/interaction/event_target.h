#pragma once

#include "domain/interaction/event_phase.h"
#include "domain/interaction/dom_event.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>

namespace ruyijs::domain::interaction {

using ListenerId = uint64_t;

/// Options for addEventListener
struct ListenerOptions {
    bool capture = false;
    bool once = false;
    bool passive = false;
    bool wants_untrusted = true;
};

/// Entity: A registered event listener on an EventTarget.
class EventListener {
public:
    EventListener(ListenerId id, EventType type, ListenerOptions options)
        : id_(id), type_(std::move(type)), options_(options) {}

    ListenerId id() const noexcept { return id_; }
    const EventType& type() const noexcept { return type_; }
    const ListenerOptions& options() const noexcept { return options_; }

    bool is_capture() const noexcept { return options_.capture; }
    bool is_once() const noexcept { return options_.once; }
    bool is_passive() const noexcept { return options_.passive; }

    /// Whether this listener has been triggered (for once semantics)
    bool triggered() const noexcept { return triggered_; }
    void mark_triggered() noexcept { triggered_ = true; }

private:
    ListenerId id_;
    EventType type_;
    ListenerOptions options_;
    bool triggered_ = false;
};

/// Aggregate Root: EventTarget
/// Manages event listeners and provides the dispatch entry point.
/// All DOM nodes, Window, XMLHttpRequest etc. are EventTargets.
class EventTargetAggregate {
public:
    explicit EventTargetAggregate(uint64_t id) : id_(id) {}

    uint64_t id() const noexcept { return id_; }

    /// Register a listener. Returns a globally unique listener ID.
    ListenerId add_listener(EventType type, ListenerOptions options = {}) {
        ListenerId id = next_global_listener_id();
        listeners_.emplace_back(id, std::move(type), options);
        return id;
    }

    /// Remove a listener by ID
    void remove_listener(ListenerId id) {
        listeners_.erase(
            std::remove_if(listeners_.begin(), listeners_.end(),
                [id](const EventListener& l) { return l.id() == id; }),
            listeners_.end());
    }

    /// Remove all listeners of a given type
    void remove_listeners_by_type(const EventType& type) {
        listeners_.erase(
            std::remove_if(listeners_.begin(), listeners_.end(),
                [&type](const EventListener& l) { return l.type() == type; }),
            listeners_.end());
    }

    /// Get all listeners matching a type and phase
    std::vector<const EventListener*> get_listeners(const EventType& type, bool capture_phase) const {
        std::vector<const EventListener*> result;
        for (const auto& l : listeners_) {
            if (l.type() == type && l.is_capture() == capture_phase && !l.triggered()) {
                result.push_back(&l);
            }
        }
        return result;
    }

    /// Mark once-listeners as triggered and remove them
    void cleanup_once_listeners() {
        listeners_.erase(
            std::remove_if(listeners_.begin(), listeners_.end(),
                [](const EventListener& l) { return l.is_once() && l.triggered(); }),
            listeners_.end());
    }

    /// Mark a specific listener as triggered
    void mark_triggered(ListenerId id) {
        for (auto& l : listeners_) {
            if (l.id() == id) {
                l.mark_triggered();
                break;
            }
        }
    }

    size_t listener_count() const noexcept { return listeners_.size(); }
    bool listener(ListenerId id, EventListener* output) const {
        for (const auto& value : listeners_) {
            if (value.id() == id && !value.triggered()) { *output = value; return true; }
        }
        return false;
    }
    std::vector<ListenerId> listener_ids(const EventType& type, bool capture) const {
        std::vector<ListenerId> ids;
        for (const auto& value : listeners_)
            if (value.type() == type && value.is_capture() == capture && !value.triggered())
                ids.push_back(value.id());
        return ids;
    }

    bool has_listeners_for(const EventType& type) const {
        return std::any_of(listeners_.begin(), listeners_.end(),
            [&type](const EventListener& l) { return l.type() == type; });
    }

    // --- Tree relationship (for propagation path) ---

    void set_parent(uint64_t parent_id) noexcept { parent_id_ = parent_id; }
    uint64_t parent_id() const noexcept { return parent_id_; }
    bool has_parent() const noexcept { return parent_id_ != 0; }

private:
    uint64_t id_;
    uint64_t parent_id_ = 0;
    std::vector<EventListener> listeners_;

    /// Global listener ID generator (ensures uniqueness across all targets)
    static ListenerId next_global_listener_id() {
        static ListenerId counter = 0;
        return ++counter;
    }
};

} // namespace ruyijs::domain::interaction
