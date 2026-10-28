#pragma once

#include "domain/shared/aggregate_root.h"
#include "domain/execution_scheduling/task.h"
#include "domain/execution_scheduling/time_point.h"
#include "domain/execution_scheduling/clock_state.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace ruyijs::domain::execution_scheduling {

using EventLoopId = uint64_t;

/// Aggregate Root: EventLoop
/// Manages the micro/macro task queues and virtual time.
/// Implements the HTML spec event loop processing model:
///   1. Pick oldest due macro-task → execute
///   2. Drain all microtasks
///   3. (Optionally) run rAF callbacks
///   4. Repeat
class EventLoopAggregate : public shared::AggregateRoot<EventLoopId> {
public:
    explicit EventLoopAggregate(EventLoopId id, TimeMode mode = TimeMode::Logical)
        : AggregateRoot<EventLoopId>(id) { clock_.mode = mode; }

    // --- Time management ---

    TimePoint current_time() const noexcept { return clock_.elapsed; }
    TimeMode time_mode() const noexcept { return clock_.mode; }
    ClockState& clock() noexcept { return clock_; }
    const ClockState& clock() const noexcept { return clock_; }
    TaskId last_task_id() const noexcept { return next_task_id_; }
    std::shared_ptr<Task> find_task(TaskId id) const noexcept {
        for (const auto& task : macrotask_queue_) if (task->id() == id) return task;
        return nullptr;
    }
    std::shared_ptr<Task> next_live_task() const noexcept {
        std::shared_ptr<Task> ordinary, idle, expired, web;
        for (const auto& task : macrotask_queue_) {
            if (task->cancelled() || task->running()) continue;
            if(task->type()==Task::Type::WebTask && task->is_due(clock_.elapsed)) {
                if(!web || task->web_priority()<web->web_priority() ||
                   (task->web_priority()==web->web_priority() && task->continuation() && !web->continuation()) ||
                   (task->web_priority()==web->web_priority() && task->continuation()==web->continuation() && task->id()<web->id()))web=task;
            }
            if (task->type() != Task::Type::IdleCallback) {
                if (!ordinary) ordinary = task;
            } else {
                if (!idle) idle = task;
                if (task->idle_timeout() <= clock_.elapsed.ms() &&
                    (!expired || task->idle_timeout() < expired->idle_timeout())) expired = task;
            }
        }
        if (expired && (!ordinary || !ordinary->is_due(clock_.elapsed) ||
                        expired->idle_timeout() < ordinary->deadline().ms())) return expired;
        if (ordinary && ordinary->is_due(clock_.elapsed)) return ordinary->type()==Task::Type::WebTask?web:ordinary;
        if (idle && (!ordinary || idle->deadline() <= ordinary->deadline())) return idle;
        return ordinary;
    }
    double idle_deadline() const noexcept {
        double end = clock_.elapsed.ms() + 50.0;
        for (const auto& task : macrotask_queue_)
            if (!task->cancelled() && !task->running() && task->type() != Task::Type::IdleCallback)
                end = std::min(end, task->deadline().ms());
        return std::max(clock_.elapsed.ms(), end);
    }
    std::shared_ptr<Task> eligible_frame(TaskId cutoff, TimePoint timestamp) const noexcept {
        for (const auto& task : macrotask_queue_)
            if (task->type() == Task::Type::AnimationFrame && !task->cancelled() &&
                !task->running() && task->id() <= cutoff && task->deadline() <= timestamp) return task;
        return nullptr;
    }

    void advance_time(double ms) {
        clock_.elapsed = TimePoint(clock_.elapsed.ms() + ms);
    }

    void set_time(TimePoint t) { clock_.elapsed = t; }

    // --- Task scheduling ---

    TaskId schedule_task(Task::Type type, TaskPriority priority, double delay_ms) {
        TaskId id = next_task_id_ + 1;
        TimePoint deadline(clock_.elapsed.ms() + delay_ms);
        auto task = std::make_shared<Task>(id, type, priority, deadline);
        if (type == Task::Type::Microtask) {
            microtask_queue_.push_back(task);
        } else {
            macrotask_queue_.push_back(task);
            sort_macrotasks();
        }
        next_task_id_ = id;
        return id;
    }

    TaskId schedule_interval(double interval_ms) {
        TaskId id = next_task_id_ + 1;
        TimePoint deadline(clock_.elapsed.ms() + interval_ms);
        auto task = std::make_shared<Task>(id, Task::Type::Interval, TaskPriority::Timer, deadline);
        task->set_interval(interval_ms);
        macrotask_queue_.push_back(task);
        sort_macrotasks();
        next_task_id_ = id;
        return id;
    }

    void cancel_task(TaskId id) {
        for (auto& task : macrotask_queue_) {
            if (task->id() == id) { task->cancel(); return; }
        }
        for (auto& task : microtask_queue_) {
            if (task->id() == id) { task->cancel(); return; }
        }
    }

    // --- Task queries ---

    /// Get all due macro-tasks at current time (ordered by deadline + priority)
    std::vector<std::shared_ptr<Task>> get_due_macrotasks() const {
        std::vector<std::shared_ptr<Task>> due;
        for (const auto& task : macrotask_queue_) {
            if (task->is_due(clock_.elapsed)) {
                due.push_back(task);
            }
        }
        return due;
    }

    /// Get all pending microtasks (FIFO order)
    std::vector<std::shared_ptr<Task>> get_pending_microtasks() const {
        std::vector<std::shared_ptr<Task>> pending;
        for (const auto& task : microtask_queue_) {
            if (!task->cancelled()) {
                pending.push_back(task);
            }
        }
        return pending;
    }

    /// Remove a task from the queue after execution
    void complete_task(TaskId id) {
        // Check if it's a repeating interval
        for (auto it = macrotask_queue_.begin(); it != macrotask_queue_.end(); ++it) {
            if ((*it)->id() == id) {
                if ((*it)->is_repeating() && !(*it)->cancelled()) {
                    (*it)->reschedule();
                    sort_macrotasks();
                } else {
                    macrotask_queue_.erase(it);
                }
                return;
            }
        }
        // Remove from microtask queue
        microtask_queue_.erase(
            std::remove_if(microtask_queue_.begin(), microtask_queue_.end(),
                [id](const auto& t) { return t->id() == id; }),
            microtask_queue_.end());
    }

    /// Remove all cancelled tasks
    void purge_cancelled() {
        macrotask_queue_.erase(
            std::remove_if(macrotask_queue_.begin(), macrotask_queue_.end(),
                [](const auto& t) { return t->cancelled(); }),
            macrotask_queue_.end());
        microtask_queue_.erase(
            std::remove_if(microtask_queue_.begin(), microtask_queue_.end(),
                [](const auto& t) { return t->cancelled(); }),
            microtask_queue_.end());
    }

    // --- Statistics ---

    bool has_pending_tasks() const {
        return !macrotask_queue_.empty() || !microtask_queue_.empty();
    }

    size_t macrotask_count() const noexcept { return macrotask_queue_.size(); }
    size_t microtask_count() const noexcept { return microtask_queue_.size(); }

    /// Next deadline (earliest due time among macrotasks)
    TimePoint next_deadline() const {
        if (macrotask_queue_.empty()) return TimePoint(1e18);
        return macrotask_queue_.front()->deadline();
    }

    // --- Auto-advance configuration ---

    double auto_advance_step() const noexcept { return auto_advance_step_; }
    void set_auto_advance_step(double ms) noexcept { auto_advance_step_ = ms; }

    double date_advance_step() const noexcept { return date_advance_step_; }
    void set_date_advance_step(double ms) noexcept { date_advance_step_ = ms; }

private:
    void sort_macrotasks() {
        std::sort(macrotask_queue_.begin(), macrotask_queue_.end(),
            [](const auto& a, const auto& b) { return *a < *b; });
    }

    ClockState clock_;
    uint64_t next_task_id_ = 0;
    std::vector<std::shared_ptr<Task>> macrotask_queue_;
    std::vector<std::shared_ptr<Task>> microtask_queue_;
    double auto_advance_step_ = 0.001;  // performance.now() increment per eval
    double date_advance_step_ = 1.0;    // Date.now() increment per eval
};

} // namespace ruyijs::domain::execution_scheduling
