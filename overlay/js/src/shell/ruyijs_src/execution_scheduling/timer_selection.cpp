#include "execution_scheduling/state.h"
#include <cmath>
#include <climits>
#include "js/Wrapper.h"

namespace ruyijs::execution_scheduling {
bool Realm::ConfigureMessageReplay(bool enabled) {
  auto& s = *state_;
  if (s.frozen) return Error(s.cx, "Message replay configuration is frozen");
  if (s.exposure != Exposure::Window) return Error(s.cx, "Message replay requires a Window owner");
  s.messageReplay = enabled; return true;
}
bool Realm::VisitPendingMessages(bool (*visitor)(void*, const MessageSnapshot&), void* context) {
  auto& s = *state_;
  if (!s.messageReplay) return Error(s.cx, "Message replay is not enabled");
  s.SyncClock();
  for (Callback* cb = s.callbacks.get(); cb; cb = cb->next.get()) {
    if (!cb->messageReplay || cb->running || !cb->global || s.Retired(cb->global)) continue;
    TaskSnapshot task; if (!s.model.TimerInfo(cb->id, &task)) {
      if (!s.model.Exists(cb->id)) continue;
      task = {cb->id, Kind::Message, s.model.Clock().now, 0};
    }
    MessageSnapshot row{cb->id, cb->messageSequence, cb->messageSourceRealm,
                        cb->messageTargetRealm, cb->messageKind, task.deadline,
                        task.deadline <= s.model.Clock().now};
    if (!visitor(context, row)) return false;
  }
  return true;
}
bool Realm::RunSelectedMessage(uint64_t id, uint32_t source, uint32_t target,
                               uint64_t sequence, uint32_t kind, double at,
                               RunResult* result) {
  auto& s = *state_; auto clock = s.model.Clock();
  if (!s.messageReplay || !clock.logical) return Error(s.cx, "Message selection requires enabled logical replay");
  if (!std::isfinite(at) || at < clock.now || at - clock.now > 86400000) return Error(s.cx, "Selected message time is invalid", true);
  Callback* cb = s.Find(id);
  if (!cb || !cb->messageReplay || cb->running || cb->messageSourceRealm != source ||
      cb->messageTargetRealm != target || cb->messageSequence != sequence || cb->messageKind != kind)
    return Error(s.cx, "Selected message identity is stale");
  if (s.microPending || s.ExternalPending()) return Error(s.cx, "Message selection has pending work");
  s.frozen = true; s.model.SetTime(at); TaskSnapshot task;
  if (!s.model.Claim(&task, false, id, true)) return Error(s.cx, "Validated message claim failed");
  s.Invoke(task); *result = {s.model.Inspect(), 1, false}; return true;
}
bool Realm::ValidateTimerRealmQuery() {
  auto& s = *state_;
  return (s.timerReplay && s.exposure == Exposure::Window) ||
         Error(s.cx, "Timer realm query requires enabled Window replay");
}
bool Realm::TimerRealmIdentity(JS::HandleObject target, uint32_t* identity) {
  auto& s = *state_;
  if (!ValidateTimerRealmQuery()) return false;
  JS::RootedObject global(s.cx, js::UncheckedUnwrap(target));
  if (!s.Owns(global) || s.Retired(global))
    return Error(s.cx, "Timer realm target must be a live installed Window global");
  s.Handles(global);
  for (size_t i = 0; i < s.publicHandles.size(); ++i) {
    if (s.publicHandles[i].global == global) { *identity = uint32_t(i + 1); return true; }
  }
  return Error(s.cx, "Installed timer realm identity missing");
}
bool Realm::ConfigureTimerReplay(bool enabled) {
  auto& s = *state_;
  if (s.frozen) return Error(s.cx, "Timer replay configuration is frozen");
  if (s.exposure != Exposure::Window) return Error(s.cx, "Timer replay requires a Window owner");
  s.timerReplay = enabled;
  return true;
}
bool Realm::VisitPendingTimers(bool (*visitor)(void*, const TimerSnapshot&), void* context) {
  auto& s = *state_;
  if (!s.timerReplay) return Error(s.cx, "Timer replay is not enabled");
  s.SyncClock();
  for (Callback* cb = s.callbacks.get(); cb; cb = cb->next.get()) {
    if (!cb->publicHandle || cb->running || !cb->global || s.Retired(cb->global) ||
        (cb->kind != Kind::Timeout && cb->kind != Kind::Interval)) continue;
    TaskSnapshot task;
    if (!s.model.TimerInfo(cb->id, &task)) continue;
    uint32_t realm = 0;
    for (size_t i = 0; i < s.publicHandles.size(); ++i)
      if (s.publicHandles[i].global == cb->global) { realm = uint32_t(i + 1); break; }
    if (!realm) return Error(s.cx, "Timer owner identity missing");
    const TimerSnapshot row{cb->id, realm, cb->publicHandle, cb->replayFirings,
                            task.deadline, cb->kind == Kind::Interval,
                            task.deadline <= s.model.Clock().now};
    if (!visitor(context, row)) return false;
  }
  return true;
}
bool Realm::ValidateTimerSelection(uint64_t id, uint32_t firings, double at) {
  auto& s = *state_;
  const auto clock = s.model.Clock();
  if (!s.timerReplay || s.exposure != Exposure::Window || !clock.logical)
    return Error(s.cx, "Timer selection requires enabled Window replay and a logical clock");
  if (!id || id > INT32_MAX || firings >= INT32_MAX)
    return Error(s.cx, "Timer selection identity exceeds bounds", true);
  if (!std::isfinite(at) || at < clock.now || at - clock.now > 86400000 || std::abs(clock.epoch + at) > 8.64e12)
    return Error(s.cx, "Selected time is outside the logical clock range", true);
  Callback* cb = s.Find(id);
  if (!cb || cb->running || !cb->publicHandle || !cb->global || s.Retired(cb->global) ||
      (cb->kind != Kind::Timeout && cb->kind != Kind::Interval))
    return Error(s.cx, "Selected public timer is not live");
  if (cb->replayFirings != firings) return Error(s.cx, "Selected timer firing count is stale");
  TaskSnapshot task;
  if (!s.model.TimerInfo(id, &task)) return Error(s.cx, "Selected timer is absent from its owner queue");
  if (task.deadline > at) return Error(s.cx, "Selected timer is not due");
  if (s.microPending || s.ExternalPending()) return Error(s.cx, "Timer selection has pending microtasks or external work");
  return true;
}
bool Realm::RunSelectedTimer(uint64_t id, uint32_t firings, double at, RunResult* result) {
  auto& s = *state_;
  if (!ValidateTimerSelection(id, firings, at)) return false;
  s.frozen = true;
  s.model.SetTime(at);
  TaskSnapshot task;
  if (!s.model.Claim(&task, false, id)) return Error(s.cx, "Validated timer claim failed");
  s.Invoke(task);
  *result = {s.model.Inspect(), 1, false};
  return true;
}
}
