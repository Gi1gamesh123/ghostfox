#pragma once

#include "domain/execution_scheduling/event_loop.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace ruyijs::domain::execution_scheduling {

/// Port (Interface): Clock abstraction.
/// Domain defines the interface; infrastructure provides implementations
/// (VirtualClock for logical mode, SystemClock for system mode).
class ClockPort {
public:
    virtual ~ClockPort() = default;

    /// Get current time in milliseconds
    virtual double now_ms() const = 0;

    /// Get Date.now() value (ms since epoch)
    virtual double date_now_ms() const = 0;
};

/// Domain Service: Task scheduler.
/// Implements the event loop processing model per HTML spec.
class TaskSchedulerService {
public:
    /// Result of running one iteration of the event loop
    struct IterationResult {
        bool executed_task = false;
        TaskId executed_task_id = 0;
        size_t microtasks_drained = 0;
    };

    /// Run one full iteration:
    ///   1. Execute oldest due macrotask
    ///   2. Drain all microtasks
    /// Returns info about what was executed.
    /// The actual callback execution is delegated to the infrastructure layer
    /// via the provided executor function.
    static IterationResult run_one_iteration(
        EventLoopAggregate& loop,
        const std::function<void(TaskId)>& executor)
    {
        IterationResult result;

        // Step 1: Find and execute the oldest due macrotask
        auto due_tasks = loop.get_due_macrotasks();
        if (!due_tasks.empty()) {
            auto& task = due_tasks.front();
            TaskId task_id = task->id();

            executor(task_id);
            loop.complete_task(task_id);

            result.executed_task = true;
            result.executed_task_id = task_id;
        }

        // Step 2: Drain all microtasks
        result.microtasks_drained = drain_microtasks(loop, executor);

        return result;
    }

    /// Drain all microtasks (Promise.then, queueMicrotask, MutationObserver)
    static size_t drain_microtasks(
        EventLoopAggregate& loop,
        const std::function<void(TaskId)>& executor)
    {
        size_t count = 0;
        // Microtasks can enqueue more microtasks, so loop until empty
        while (true) {
            auto pending = loop.get_pending_microtasks();
            if (pending.empty()) break;

            for (auto& task : pending) {
                executor(task->id());
                loop.complete_task(task->id());
                ++count;
            }
        }
        return count;
    }

    /// Advance time by total_ms in steps, executing tasks as they become due.
    /// Simulates rAF rhythm with default step ~16.67ms.
    static size_t advance(
        EventLoopAggregate& loop,
        double total_ms,
        double step_ms,
        const std::function<void(TaskId)>& executor)
    {
        size_t tasks_executed = 0;
        double elapsed = 0.0;

        while (elapsed < total_ms) {
            double step = (elapsed + step_ms > total_ms) ? (total_ms - elapsed) : step_ms;
            loop.advance_time(step);
            elapsed += step;

            // Execute all due tasks at this time point
            while (true) {
                auto result = run_one_iteration(loop, executor);
                if (!result.executed_task && result.microtasks_drained == 0) break;
                if (result.executed_task) ++tasks_executed;
            }
        }

        return tasks_executed;
    }

    /// Sleep: advance time by ms, executing tasks in timeline order.
    static size_t sleep(
        EventLoopAggregate& loop,
        double ms,
        const std::function<void(TaskId)>& executor)
    {
        return advance(loop, ms, Duration::FRAME_MS, executor);
    }

    /// Drain: execute all currently due tasks without advancing time.
    static size_t drain(
        EventLoopAggregate& loop,
        const std::function<void(TaskId)>& executor,
        size_t max_tasks = SIZE_MAX)
    {
        size_t count = 0;
        while (count < max_tasks) {
            auto result = run_one_iteration(loop, executor);
            if (!result.executed_task && result.microtasks_drained == 0) break;
            if (result.executed_task) ++count;
        }
        return count;
    }
};

/// Domain Service: Time control.
/// Manages virtual time advancement and Date.now()/performance.now() interception.
class TimeControlService {
public:
    /// Compute the next performance.now() value after an eval
    static double next_performance_now(const EventLoopAggregate& loop) {
        return loop.current_time().ms() + loop.auto_advance_step();
    }

    /// Compute the next Date.now() value after an eval
    /// (base epoch + virtual elapsed + step)
    static double next_date_now(double base_epoch_ms, const EventLoopAggregate& loop) {
        return base_epoch_ms + loop.current_time().ms() + loop.date_advance_step();
    }

    /// Determine how much time to advance to reach the next scheduled task
    static double time_until_next_task(const EventLoopAggregate& loop) {
        if (!loop.has_pending_tasks()) return 0.0;
        double next = loop.next_deadline().ms();
        double current = loop.current_time().ms();
        return (next > current) ? (next - current) : 0.0;
    }
};

} // namespace ruyijs::domain::execution_scheduling
