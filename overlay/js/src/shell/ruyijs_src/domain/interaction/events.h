#pragma once

#include "domain/shared/domain_event.h"

#include <cstdint>
#include <string>

namespace ruyijs::domain::interaction {

/// Domain Event: Raised when an event is dispatched through the DOM.
class EventDispatchedDomainEvent : public shared::DomainEvent {
public:
    EventDispatchedDomainEvent(uint64_t target_id, std::string event_type, bool trusted)
        : DomainEvent("EventDispatched"),
          target_id_(target_id),
          event_type_(std::move(event_type)),
          trusted_(trusted) {}

    uint64_t target_id() const noexcept { return target_id_; }
    const std::string& dispatched_event_type() const noexcept { return event_type_; }
    bool trusted() const noexcept { return trusted_; }

private:
    uint64_t target_id_;
    std::string event_type_;
    bool trusted_;
};

} // namespace ruyijs::domain::interaction
