#include "execution_scheduling/state.h"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace ruyijs::execution_scheduling {
bool State::RunExternal() {
  if (!external.run || !external.run(external.context)) return false;
  CheckpointJobs();
  return true;
}
bool State::AwaitExternal(double until, double step) {
  if (!ExternalPending()) return false;
  if (model.Clock().logical) {
    while (ExternalPending()) if (ExternalReady(1000)) return true;
    return false;
  }
  WaitUntil(until, step);
  return ExternalReady(0);
}
void Realm::SetExternalWork(const ExternalWork& work) { state_->external = work; }
bool Realm::RunExternalTask() {
  auto& s = *state_;
  // Cap at 10 seconds: on platforms where the async transport's completion
  // signal never arrives (Wine), this prevents an infinite spin.
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  for (;;) {
    if (s.RunExternal()) return true;
    if (!s.ExternalPending()) return false;
    if (std::chrono::steady_clock::now() > deadline) return false;
    s.ExternalReady(1000);
  }
}
bool Realm::ValidateAdvance(double ms, double step, uint32_t maxTasks) {
  const auto clock = state_->model.Clock();
  if (!std::isfinite(ms) || ms < 0 || ms > (clock.logical ? 86400000 : 60000))
    return Error(state_->cx, "Advance duration is outside the clock mode limit", true);
  if (!std::isfinite(step) || step <= 0 || step > 86400000)
    return Error(state_->cx, "Advance step must be positive and at most one day", true);
  if (maxTasks < 1 || maxTasks > 100000)
    return Error(state_->cx, "Task budget must be an integer from 1 to 100000", true);
  if (clock.logical && std::abs(clock.epoch + clock.now + ms) > 8.64e12)
    return Error(state_->cx, "Advance exceeds the exact logical millisecond range", true);
  return true;
}
RunResult Realm::Advance(double ms, double step, uint32_t maxTasks) {
  auto& s = *state_;
  s.frozen = true;
  s.SyncClock();
  const double target = s.model.Clock().now + ms;
  s.CheckpointJobs();
  uint32_t executed = 0;
  bool exhausted = false;
  while (true) {
    s.SyncClock();
    if (executed < maxTasks && s.RunExternal()) { ++executed; continue; }
    auto now = s.model.Inspect();
    const bool frameBatch = s.model.FramePending();
    if (executed >= maxTasks) {
      exhausted = frameBatch || (now.has_next && now.next <= target) || s.ExternalReady(0);
      if (!exhausted && now.now < target) s.WaitUntil(target, step);
      break;
    }
    if (!frameBatch) {
      if (!now.has_next || now.next > target) {
        if (s.AwaitExternal(target, step)) continue;
        if (now.now < target) s.WaitUntil(target, step);
        break;
      }
      // Empty logical steps have no observable work, so coalesce them to the
      // next live deadline. System waits use step with a 50 ms slice ceiling.
      // A macro-task boundary is not a millisecond of elapsed time. Keep
      // draining due tasks (including those at target) at the current time.
      // Invoke checkpoints engine jobs after each individual callback.
      const double turn = std::max(now.now, now.next);
      if (turn > now.now) {
        if (s.AwaitExternal(turn, step)) continue;
        s.WaitUntil(turn, step);
      }
    }
    TaskSnapshot task;
    if (!s.model.Claim(&task, frameBatch)) break;
    s.Invoke(task);
    ++executed;
  }
  s.SyncClock();
  return {s.model.Inspect(), executed, exhausted};
}
RunResult Realm::Drain(uint32_t maxTasks, bool (*continueTasks)(void*), void* context) {
  auto& s = *state_;
  s.frozen = true;
  s.SyncClock();
  s.CheckpointJobs();
  uint32_t executed = 0;
  while (executed < maxTasks && (!continueTasks || continueTasks(context))) {
    s.SyncClock();
    if (s.RunExternal()) { ++executed; continue; }
    TaskSnapshot task;
    if (!s.model.Claim(&task, s.model.FramePending())) {
      // Logical time: a pending transport finishes at this instant.
      if (s.model.Clock().logical && s.AwaitExternal(0, 16.667)) continue;
      break;
    }
    s.Invoke(task);
    ++executed;
  }
  s.SyncClock();
  const auto now = s.model.Inspect();
  return {now, executed, executed == maxTasks && now.has_next && now.next <= now.now};
}
}
