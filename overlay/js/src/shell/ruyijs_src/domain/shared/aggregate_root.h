#pragma once

#include "domain/shared/entity.h"

#include <memory>
#include <vector>

namespace ruyijs::domain::shared {

// Forward declaration
class DomainEvent;

/// Base class for Aggregate Roots.
/// An Aggregate Root is the entry point to an aggregate boundary.
/// External objects may only reference the aggregate root by ID.
/// All modifications to internal entities must go through the root.
template <typename ID>
class AggregateRoot : public Entity<ID> {
public:
    explicit AggregateRoot(ID id) : Entity<ID>(std::move(id)) {}

    ~AggregateRoot() override = default;

    /// Collect domain events raised during aggregate operations.
    /// Events are consumed (cleared) after being dispatched by the application layer.
    const std::vector<std::shared_ptr<DomainEvent>>& domain_events() const noexcept {
        return domain_events_;
    }

    /// Clear all pending domain events (called after dispatch)
    void clear_domain_events() noexcept { domain_events_.clear(); }

protected:
    /// Raise a domain event within the aggregate boundary
    void raise_event(std::shared_ptr<DomainEvent> event) {
        domain_events_.push_back(std::move(event));
    }

    AggregateRoot(const AggregateRoot&) = default;
    AggregateRoot& operator=(const AggregateRoot&) = default;
    AggregateRoot(AggregateRoot&&) noexcept = default;
    AggregateRoot& operator=(AggregateRoot&&) noexcept = default;

private:
    std::vector<std::shared_ptr<DomainEvent>> domain_events_;
};

} // namespace ruyijs::domain::shared
