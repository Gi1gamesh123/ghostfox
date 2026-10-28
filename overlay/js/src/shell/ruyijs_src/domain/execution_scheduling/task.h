#pragma once

#include "domain/shared/entity.h"
#include "domain/execution_scheduling/time_point.h"

#include <cstdint>
#include <functional>
#include <string>

namespace ruyijs::domain::execution_scheduling {

using TaskId = uint64_t;

/// Entity: A scheduled task in the event loop.
/// Represents setTimeout/setInterval/rAF/XHR callbacks etc.
class Task : public shared::Entity<TaskId> {
public:
    enum class Type {
        Timeout,        // setTimeout (one-shot)
        Interval,       // setInterval (repeating)
        AnimationFrame, // requestAnimationFrame
        IdleCallback,   // requestIdleCallback
        NetworkCallback,// XHR/fetch response
        Microtask,      // Promise.then / queueMicrotask
        WebTask,        // Scheduler.postTask / yield
    };

    Task(TaskId id, Type type, TaskPriority priority, TimePoint deadline)
        : Entity<TaskId>(id), type_(type), priority_(priority), deadline_(deadline) {}

    Type type() const noexcept { return type_; }
    TaskPriority priority() const noexcept { return priority_; }
    TimePoint deadline() const noexcept { return deadline_; }

    /// For setInterval: the repeat interval in ms
    double interval_ms() const noexcept { return interval_ms_; }
    void set_interval(double ms) noexcept { interval_ms_ = ms; }

    bool is_repeating() const noexcept { return type_ == Type::Interval; }
    bool is_microtask() const noexcept { return type_ == Type::Microtask; }

    /// Whether this task has been cancelled (clearTimeout/clearInterval)
    bool cancelled() const noexcept { return cancelled_; }
    void cancel() noexcept { cancelled_ = true; }
    bool running() const noexcept { return running_; }
    void set_running(bool value) noexcept { running_ = value; }
    TimePoint running_started() const noexcept { return running_started_; }
    void set_running_started(TimePoint value) noexcept { running_started_ = value; }
    unsigned nesting() const noexcept { return nesting_; }
    void set_nesting(unsigned value) noexcept { nesting_ = value; }
    void set_deadline(TimePoint value) noexcept { deadline_ = value; }
    double idle_timeout() const noexcept { return idle_timeout_; }
    void set_idle_timeout(double value) noexcept { idle_timeout_ = value; }
    int web_priority() const noexcept { return web_priority_; }
    bool continuation() const noexcept { return continuation_; }
    void set_web_priority(int priority,bool continuation) noexcept {web_priority_=priority;continuation_=continuation;}

    /// Whether this task is due at the given time
    bool is_due(TimePoint now) const noexcept {
        return !cancelled_ && !running_ && deadline_ <= now;
    }

    /// Reschedule for next interval (for repeating tasks)
    void reschedule() {
        if (is_repeating()) {
            deadline_ = TimePoint(deadline_.ms() + interval_ms_);
        }
    }

    /// Extra arguments passed to setTimeout(fn, delay, ...args)
    const std::string& args_json() const noexcept { return args_json_; }
    void set_args_json(std::string args) { args_json_ = std::move(args); }

    /// Ordering: earlier deadline first, then higher priority (lower value)
    bool operator<(const Task& other) const {
        if (deadline_ != other.deadline_) return deadline_ < other.deadline_;
        if (priority_ != other.priority_)
            return static_cast<uint8_t>(priority_) < static_cast<uint8_t>(other.priority_);
        return id() < other.id();
    }

private:
    Type type_;
    TaskPriority priority_;
    TimePoint deadline_;
    double interval_ms_ = 0.0;
    bool cancelled_ = false;
    bool running_ = false;
    TimePoint running_started_;
    unsigned nesting_ = 0;
    double idle_timeout_ = 1e18;
    int web_priority_ = 1;
    bool continuation_ = false;
    std::string args_json_;
};

} // namespace ruyijs::domain::execution_scheduling
