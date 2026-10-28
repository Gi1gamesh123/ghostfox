#pragma once

#include "domain/interaction/dom_event.h"
#include "domain/interaction/event_phase.h"

#include <memory>
#include <string>

namespace ruyijs::domain::interaction {

// MouseEventInit/KeyboardEventInit live with the other event init dictionaries
// in domain/interaction/dom_event.h; the input simulation reads the same values.

/// Domain Service: Input simulation.
/// Creates trusted (isTrusted=true) input events that behave identically
/// to real user input from the JS perspective.
class InputSimulationService {
public:
    /// Create a trusted mouse event from init parameters.
    /// The event is marked as trusted (isTrusted=true).
    static std::unique_ptr<MouseEventDomain> create_trusted_mouse_event(
        const MouseEventInit& init)
    {
        auto event = std::make_unique<MouseEventDomain>(
            EventType(init.type),
            init.client_x, init.client_y,
            init.screen_x, init.screen_y,
            init.button, init.buttons,
            init.ctrl_key, init.shift_key, init.alt_key, init.meta_key
        );
        event->mark_trusted();
        event->set_composed(true);
        return event;
    }

    /// Create a trusted pointer event (same as mouse but with pointer semantics).
    static std::unique_ptr<MouseEventDomain> create_trusted_pointer_event(
        const MouseEventInit& init)
    {
        // PointerEvent extends MouseEvent in the spec;
        // at domain level we reuse MouseEventDomain with pointer type
        auto event = std::make_unique<MouseEventDomain>(
            EventType(init.type),
            init.client_x, init.client_y,
            init.screen_x, init.screen_y,
            init.button, init.buttons,
            init.ctrl_key, init.shift_key, init.alt_key, init.meta_key
        );
        event->mark_trusted();
        event->set_composed(true);
        return event;
    }

    /// Create a trusted keyboard event.
    static std::unique_ptr<KeyboardEventDomain> create_trusted_keyboard_event(
        const KeyboardEventInit& init)
    {
        auto event = std::make_unique<KeyboardEventDomain>(
            EventType(init.type),
            init.key, init.code,
            init.ctrl_key, init.shift_key, init.alt_key, init.meta_key
        );
        event->mark_trusted();
        event->set_composed(true);
        return event;
    }

    /// Validate that a mouse event type is a supported input type
    static bool is_supported_mouse_type(const std::string& type) {
        return type == "click" || type == "dblclick" ||
               type == "mousedown" || type == "mouseup" ||
               type == "mousemove" || type == "mouseenter" ||
               type == "mouseleave" || type == "mouseover" ||
               type == "mouseout" || type == "contextmenu" ||
               type == "pointerdown" || type == "pointerup" ||
               type == "pointermove" || type == "pointerenter" ||
               type == "pointerleave" || type == "pointerover" ||
               type == "pointerout" || type == "pointercancel";
    }
};

} // namespace ruyijs::domain::interaction
