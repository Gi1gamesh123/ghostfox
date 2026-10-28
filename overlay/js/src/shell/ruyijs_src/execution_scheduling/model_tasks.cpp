#include "execution_scheduling/model_internal.h"
#include <algorithm>
#include <climits>
#include <cmath>

namespace ruyijs::execution_scheduling {
Status Model::Schedule(Kind kind, int32_t delay, uint64_t* id) noexcept {
  auto& loop = impl_->loop;
  if (loop.macrotask_count() >= 10000) return Range("Pending timer limit exceeded");
  if (loop.last_task_id() >= INT32_MAX) return Range("Timer identifier limit exceeded");
  const bool timer = kind == Kind::Timeout || kind == Kind::Interval;
  const unsigned nesting = std::min(5u, loop.clock().running_nesting + 1);
  double duration = std::max(0, delay);
  if (timer && nesting >= 5) duration = std::max(4.0, duration);
  if (kind == Kind::Frame) {
    const double now = loop.current_time().ms();
    duration = (std::floor(now / domain::Duration::FRAME_MS) + 1) * domain::Duration::FRAME_MS - now;
  }
  try {
    const domain::Task::Type type =
        kind == Kind::Frame ? domain::Task::Type::AnimationFrame :
        kind == Kind::Message ? domain::Task::Type::NetworkCallback :
        domain::Task::Type::Timeout;
    const domain::TaskPriority priority =
        kind == Kind::Frame ? domain::TaskPriority::Animation :
        kind == Kind::Message ? domain::TaskPriority::Network :
        domain::TaskPriority::Timer;
    auto value = kind == Kind::Interval ? loop.schedule_interval(duration) :
        loop.schedule_task(type, priority, duration);
    loop.find_task(value)->set_nesting(timer ? nesting : 0);
    *id = value;
    return {};
  } catch (const std::bad_alloc&) { return OOM(); }
  catch (...) { return {ErrorCode::InvalidState, "Timer registration failed"}; }
}
bool Model::FramePending() const noexcept {
  const auto& loop = impl_->loop;
  return bool(loop.eligible_frame(loop.clock().frame_cutoff, domain::TimePoint(loop.clock().frame_timestamp)));
}
bool Model::TimerInfo(uint64_t id, TaskSnapshot* output) const noexcept {
  auto task = impl_->loop.find_task(id);
  if (!task || task->cancelled() || task->running() ||
      (task->type() != domain::Task::Type::Timeout && task->type() != domain::Task::Type::Interval)) return false;
  *output = {id, task->is_repeating() ? Kind::Interval : Kind::Timeout, task->deadline().ms()};
  return true;
}
bool Model::Claim(TaskSnapshot* output, bool frameOnly, uint64_t selected, bool selectedMessage) noexcept {
  auto& loop = impl_->loop;
  auto task = selected ? loop.find_task(selected) : frameOnly ? loop.eligible_frame(loop.clock().frame_cutoff,
      domain::TimePoint(loop.clock().frame_timestamp)) : loop.next_live_task();
  if (!task || task->cancelled() || task->running() || !task->is_due(loop.current_time())) return false;
  if (selected && (!selectedMessage && task->type() != domain::Task::Type::Timeout && task->type() != domain::Task::Type::Interval)) return false;
  task->set_running(true);
  task->set_running_started(loop.current_time());
  loop.clock().running_nesting = task->nesting();
  const bool frame = task->type() == domain::Task::Type::AnimationFrame;
  if (frame && (task->id() > loop.clock().frame_cutoff ||
                task->deadline().ms() > loop.clock().frame_timestamp)) {
    loop.clock().frame_timestamp = loop.current_time().ms();
    loop.clock().frame_cutoff = loop.last_task_id();
  }
  *output = {task->id(), frame ? Kind::Frame : task->is_repeating() ? Kind::Interval : Kind::Timeout,
             task->deadline().ms(), frame ? loop.clock().frame_timestamp : 0};
  if(task->type()==domain::Task::Type::IdleCallback){
    loop.clock().idle_running=true;
    output->kind=Kind::Idle;
    output->didTimeout=task->idle_timeout()<=loop.current_time().ms();
    if(!output->didTimeout){
      if(loop.clock().idle_end<=loop.current_time().ms()){
        loop.clock().idle_end=loop.idle_deadline();loop.clock().idle_cutoff=loop.last_task_id();
      }
      output->idleUntil=std::min(loop.clock().idle_end,loop.idle_deadline());
    }
  }
  if(task->type()==domain::Task::Type::WebTask)output->kind=Kind::WebTask;
  return true;
}
void Model::Complete(uint64_t id) noexcept {
  auto& loop = impl_->loop;
  auto task = loop.find_task(id);
  loop.clock().running_nesting = 0;
  loop.clock().idle_running = false;
  if (!task) return;
  task->set_running(false);
  if (task->is_repeating()) {
    task->set_nesting(std::min(5u, task->nesting() + 1));
    if (task->nesting() >= 5) task->set_interval(std::max(4.0, task->interval_ms()));
    const double next = std::max(loop.current_time().ms(),
        task->running_started().ms() + task->interval_ms());
    task->set_deadline(domain::TimePoint(next - task->interval_ms()));
  }
  loop.complete_task(id);
}
}
