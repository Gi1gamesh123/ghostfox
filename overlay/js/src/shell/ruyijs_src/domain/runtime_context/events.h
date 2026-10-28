#pragma once

#include "domain/shared/domain_event.h"
#include "domain/runtime_context/runtime_context.h"

#include <string>

namespace ruyijs::domain::runtime_context {

/// Domain Event: Raised when a new RuntimeContext is created and ready.
class ContextCreatedEvent : public shared::DomainEvent {
public:
    explicit ContextCreatedEvent(ContextId context_id, ContextMode mode)
        : DomainEvent("ContextCreated"),
          context_id_(context_id), mode_(mode) {}

    ContextId context_id() const noexcept { return context_id_; }
    ContextMode mode() const noexcept { return mode_; }

private:
    ContextId context_id_;
    ContextMode mode_;
};

/// Domain Event: Raised when a RuntimeContext is closed.
class ContextDestroyedEvent : public shared::DomainEvent {
public:
    explicit ContextDestroyedEvent(ContextId context_id, uint64_t total_evals)
        : DomainEvent("ContextDestroyed"),
          context_id_(context_id), total_evals_(total_evals) {}

    ContextId context_id() const noexcept { return context_id_; }
    uint64_t total_evals() const noexcept { return total_evals_; }

private:
    ContextId context_id_;
    uint64_t total_evals_;
};

/// Domain Event: Raised when a JavaScript evaluation completes (success or error).
class EvalCompletedEvent : public shared::DomainEvent {
public:
    EvalCompletedEvent(ContextId context_id, std::string script_name, bool success)
        : DomainEvent("EvalCompleted"),
          context_id_(context_id),
          script_name_(std::move(script_name)),
          success_(success) {}

    ContextId context_id() const noexcept { return context_id_; }
    const std::string& script_name() const noexcept { return script_name_; }
    bool success() const noexcept { return success_; }

private:
    ContextId context_id_;
    std::string script_name_;
    bool success_;
};

} // namespace ruyijs::domain::runtime_context
