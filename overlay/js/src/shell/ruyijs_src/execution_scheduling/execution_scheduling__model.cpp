#include "execution_scheduling/model_internal.h"

namespace ruyijs::execution_scheduling {
Model::~Model() noexcept { delete impl_; }
Status Model::Initialize(double epoch) noexcept {
  try {
    impl_ = new Impl;
    impl_->loop.clock().epoch_ms = epoch;
    return {};
  } catch (const std::bad_alloc&) { return OOM(); }
  catch (...) { return {ErrorCode::InvalidState, "Scheduling initialization failed"}; }
}
Status Model::Configure(bool logical, double epoch) noexcept {
  if (!std::isfinite(epoch) || std::trunc(epoch) != epoch || std::abs(epoch) > 8.64e12)
    return Range("Clock epoch is outside the exact logical millisecond range");
  auto& loop = impl_->loop;
  // Runtime owners may publish platform data while the shell is starting. In
  // particular SpeechSynthesis queues its one voiceschanged publication task
  // before the host can select the clock mode. Clock selection is still frozen
  // after the first page operation, so retain those bootstrap tasks here and
  // rebase the shared elapsed time into the selected clock domain.
  loop.clock().mode = logical ? domain::TimeMode::Logical : domain::TimeMode::System;
  loop.clock().epoch_ms = epoch;
  loop.clock().running_nesting = 0;
  loop.clock().frame_timestamp = -1;
  loop.clock().frame_cutoff = 0;
  loop.clock().idle_end = 0;
  loop.clock().idle_cutoff = 0;
  loop.set_time(domain::TimePoint(0));
  return {};
}
Snapshot Model::Inspect() const noexcept {
  const auto& loop = impl_->loop;
  auto task = loop.next_live_task();
  return {loop.time_mode() == domain::TimeMode::Logical, loop.current_time().ms(),
          loop.clock().epoch_ms, task ? task->deadline().ms() : 0,
          loop.macrotask_count(), bool(task)};
}
ClockSnapshot Model::Clock() const noexcept {
  const auto& clock = impl_->loop.clock();
  return {clock.mode == domain::TimeMode::Logical, clock.elapsed.ms(), clock.epoch_ms};
}
void Model::SetTime(double now) noexcept { impl_->loop.set_time(domain::TimePoint(now)); }
bool Model::Exists(uint64_t id) const noexcept {
  auto task = impl_->loop.find_task(id);
  return task && !task->cancelled();
}
void Model::Cancel(uint64_t id, bool frame) noexcept {
  auto& loop = impl_->loop;
  auto task = loop.find_task(id);
  if (!task || task->type() == domain::Task::Type::IdleCallback || task->type() == domain::Task::Type::WebTask ||
      (task->type() == domain::Task::Type::AnimationFrame) != frame) return;
  loop.cancel_task(id);
  loop.purge_cancelled();
}
}
