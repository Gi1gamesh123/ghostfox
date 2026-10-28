#pragma once

#include "application/domain.h"
#include "domain/execution_scheduling/task_scheduler_service.h"

namespace ruyijs::application {

class EventLoopControlAppService {
public:
    using TaskExecutor = std::function<void(domain::execution_scheduling::TaskId)>;

    /// advance(total, step?) — advance virtual time in frames
    static size_t advance(domain::execution_scheduling::EventLoopAggregate& loop,
                          double total_ms, double step_ms,
                          const TaskExecutor& executor) {
        return domain::execution_scheduling::TaskSchedulerService::advance(
            loop, total_ms, step_ms, executor);
    }

    /// sleep(ms) — advance time and execute tasks in order
    static size_t sleep(domain::execution_scheduling::EventLoopAggregate& loop,
                        double ms, const TaskExecutor& executor) {
        return domain::execution_scheduling::TaskSchedulerService::sleep(loop, ms, executor);
    }

    /// drain(max?) — execute all due tasks without advancing time
    static size_t drain(domain::execution_scheduling::EventLoopAggregate& loop,
                        const TaskExecutor& executor, size_t max = SIZE_MAX) {
        return domain::execution_scheduling::TaskSchedulerService::drain(loop, executor, max);
    }

    /// drainMicrotasks() — only drain microtask queue
    static size_t drain_microtasks(domain::execution_scheduling::EventLoopAggregate& loop,
                                   const TaskExecutor& executor) {
        return domain::execution_scheduling::TaskSchedulerService::drain_microtasks(loop, executor);
    }

    /// tick(ms?) — advance time by ms and run one iteration
    static void tick(domain::execution_scheduling::EventLoopAggregate& loop,
                     double ms, const TaskExecutor& executor) {
        loop.advance_time(ms);
        domain::execution_scheduling::TaskSchedulerService::run_one_iteration(loop, executor);
    }
};

} // namespace ruyijs::application
