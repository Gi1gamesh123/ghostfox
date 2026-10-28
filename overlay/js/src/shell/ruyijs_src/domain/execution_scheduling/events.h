#pragma once

#include "domain/shared/domain_event.h"
#include "domain/execution_scheduling/task.h"

namespace ruyijs::domain::execution_scheduling {

/// Domain Event: A task was executed.
class TaskExecutedEvent : public shared::DomainEvent {
public:
    TaskExecutedEvent(TaskId task_id, Task::Type type)
        : DomainEvent("TaskExecuted"), task_id_(task_id), type_(type) {}

    TaskId task_id() const noexcept { return task_id_; }
    Task::Type task_type() const noexcept { return type_; }

private:
    TaskId task_id_;
    Task::Type type_;
};

/// Domain Event: Virtual time was advanced.
class TimeAdvancedEvent : public shared::DomainEvent {
public:
    TimeAdvancedEvent(double from_ms, double to_ms)
        : DomainEvent("TimeAdvanced"), from_ms_(from_ms), to_ms_(to_ms) {}

    double from_ms() const noexcept { return from_ms_; }
    double to_ms() const noexcept { return to_ms_; }
    double delta_ms() const noexcept { return to_ms_ - from_ms_; }

private:
    double from_ms_;
    double to_ms_;
};

} // namespace ruyijs::domain::execution_scheduling
