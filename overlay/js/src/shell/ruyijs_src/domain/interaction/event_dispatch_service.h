#pragma once

#include "domain/interaction/event_target.h"
#include "domain/interaction/dom_event.h"

#include <functional>
#include <vector>
#include <unordered_map>

namespace ruyijs::domain::interaction {

/// Callback invoked when a listener fires.
/// The infrastructure layer provides the actual V8 callback bridge.
using ListenerCallback = std::function<void(DomEvent& event, uint64_t target_id)>;

/// Domain Service: Event dispatch algorithm.
/// Implements the W3C event flow: capture → target → bubble.
/// This is a pure domain algorithm — no V8 dependency.
class EventDispatchService {
public:
    /// Dispatch an event through a propagation path.
    ///
    /// @param event         The event to dispatch
    /// @param path          Ordered list of EventTarget IDs from root → target
    /// @param targets       Map of target ID → EventTarget aggregate
    /// @param callbacks     Map of listener ID → callback function
    /// @return              true if event was NOT cancelled (default not prevented)
    ///
    /// Algorithm (per DOM spec):
    ///   1. Capture phase: traverse path from root to target's parent,
    ///      invoke capture listeners
    ///   2. Target phase: invoke all listeners on target (both capture & bubble)
    ///   3. Bubble phase: traverse path from target's parent to root,
    ///      invoke bubble listeners (only if event.bubbles)
    static bool dispatch(
        DomEvent& event,
        const std::vector<uint64_t>& path,
        std::unordered_map<uint64_t, EventTargetAggregate*>& targets,
        const std::unordered_map<ListenerId, ListenerCallback>& callbacks)
    {
        return dispatch_resolved(event, path,
            [&](uint64_t id) -> EventTargetAggregate* {
                auto entry = targets.find(id);
                return entry == targets.end() ? nullptr : entry->second;
            },
            [&](ListenerId id, uint64_t) -> ListenerCallback {
                auto entry = callbacks.find(id);
                return entry == callbacks.end() ? ListenerCallback() : entry->second;
            });
    }

    // Resolve the original aggregate and each live callback at invocation time.
    // Lookup providers can span registries without copying listener state.
    template<class TargetLookup, class CallbackLookup>
    static bool dispatch_resolved(DomEvent& event, const std::vector<uint64_t>& path,
                                  const TargetLookup& targets, const CallbackLookup& callbacks)
    {
        return dispatch_resolved(event, path, targets, callbacks,
            [&](size_t index) { return index + 1 == path.size(); },
            [](size_t) {});
    }

    // The existing dispatcher consumes native path facts; it does not own a DOM
    // tree or store per-position projections after this synchronous dispatch.
    template<class TargetLookup, class CallbackLookup, class AtTarget, class BeforePosition>
    static bool dispatch_resolved(DomEvent& event, const std::vector<uint64_t>& path,
                                  const TargetLookup& targets, const CallbackLookup& callbacks,
                                  const AtTarget& at_target, const BeforePosition& before_position)
    {
        if (event.dispatching() || !event.initialized()) return false;
        event.begin_dispatch(path.empty() ? 0 : path.back());
        for (size_t index = 0; index < path.size(); ++index) {
            if (event.propagation_stopped()) break;
            before_position(index);
            event.set_event_phase(at_target(index) ? EventPhase::AtTarget : EventPhase::Capturing);
            invoke_listeners(event, path[index], true, targets, callbacks);
        }
        for (size_t index = path.size(); index > 0; --index) {
            if (event.propagation_stopped()) break;
            const size_t position = index - 1;
            if (!at_target(position) && !event.bubbles()) continue;
            before_position(position);
            event.set_event_phase(at_target(position) ? EventPhase::AtTarget : EventPhase::Bubbling);
            invoke_listeners(event, path[position], false, targets, callbacks);
        }
        event.end_dispatch();
        return !event.default_prevented();
    }

    /// Build propagation path from target up to root using parent links.
    static std::vector<uint64_t> build_path(
        uint64_t target_id,
        const std::unordered_map<uint64_t, EventTargetAggregate*>& targets)
    {
        std::vector<uint64_t> path;
        uint64_t current = target_id;

        // Walk up from target to root
        while (current != 0 && path.size() < 100000 &&
               std::find(path.begin(), path.end(), current) == path.end()) {
            path.push_back(current);
            auto it = targets.find(current);
            if (it == targets.end() || !it->second->has_parent()) break;
            current = it->second->parent_id();
        }

        // Reverse to get root → target order
        std::reverse(path.begin(), path.end());
        return path;
    }

private:
    template<class TargetLookup, class CallbackLookup>
    static void invoke_listeners(
        DomEvent& event,
        uint64_t target_id,
        bool capture_phase,
        const TargetLookup& targets,
        const CallbackLookup& callbacks)
    {
        auto* target = targets(target_id);
        if (!target) return;
        auto listeners = target->listener_ids(event.type(), capture_phase);

        for (auto id : listeners) {
            if (event.immediate_propagation_stopped()) break;
            EventListener listener(id, EventType(), {});
            if (!target->listener(id, &listener)) continue;
            if (!event.is_trusted() && !listener.options().wants_untrusted) continue;
            ListenerCallback callback = callbacks(id, target_id);
            if (listener.is_once()) target->remove_listener(id);
            event.set_current_target(target_id);
            event.set_passive(listener.is_passive());
            if (callback) callback(event, target_id);
            event.set_passive(false);
        }

        // Cleanup once listeners after invocation
        target->cleanup_once_listeners();
    }
};

} // namespace ruyijs::domain::interaction
