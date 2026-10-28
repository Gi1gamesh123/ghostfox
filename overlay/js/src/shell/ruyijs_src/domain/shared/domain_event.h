#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace ruyijs::domain::shared {

/// Base class for all Domain Events.
/// Domain events represent something meaningful that happened in the domain.
/// They are immutable and used for cross-aggregate communication.
class DomainEvent {
public:
    explicit DomainEvent(std::string event_type)
        : event_type_(std::move(event_type)),
          timestamp_(std::chrono::steady_clock::now()),
          sequence_(next_sequence()) {}

    virtual ~DomainEvent() = default;

    /// The type name of this event (e.g., "ContextCreated", "NodeInserted")
    const std::string& event_type() const noexcept { return event_type_; }

    /// Monotonic timestamp when the event was created
    std::chrono::steady_clock::time_point timestamp() const noexcept { return timestamp_; }

    /// Global sequence number for ordering
    uint64_t sequence() const noexcept { return sequence_; }

protected:
    DomainEvent(const DomainEvent&) = default;
    DomainEvent& operator=(const DomainEvent&) = default;

private:
    static uint64_t next_sequence() {
        static uint64_t counter = 0;
        return ++counter;
    }

    std::string event_type_;
    std::chrono::steady_clock::time_point timestamp_;
    uint64_t sequence_;
};

} // namespace ruyijs::domain::shared
