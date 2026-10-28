// Playback control (web-animations-1 §4.4.4-4.4.17): play, pause, finish,
// cancel, reverse, and the current time, start time and playback rate setters,
// with Firefox's error text.
#include "web_animation/animation_internal.h"
#include "js/Conversions.h"
#include <cmath>
#include <cstring>

namespace ruyijs::web_animation {
bool SilentlySetCurrentTime(State& s, JS::HandleObject a, std::optional<double> seek, const char* prefix) {
  if (!seek) {
    if (CurrentTime(a))
      return TypeFail(s.cx, std::string(prefix) + "Current time is resolved but trying to set it to an unresolved time");
    return true;
  }
  const auto now = TimelineTime(TimelineOf(a));
  if (Opt(a, kAHold) || !Opt(a, kAStart) || !now || Rate(a) == 0) SetOpt(a, kAHold, seek);
  else SetOpt(a, kAStart, *now - *seek / Rate(a));
  if (!now) SetOpt(a, kAStart, std::nullopt);
  SetOpt(a, kAPrevious, std::nullopt);
  return true;
}

bool SetCurrentTime(State& s, JS::HandleObject a, JS::HandleValue value) {
  std::optional<double> seek;
  if (!value.isNull()) {
    double ms = 0;
    if (!JS::ToNumber(s.cx, value, &ms)) return false;
    if (!std::isfinite(ms))
      return TypeFail(s.cx, "Animation.currentTime setter: Double branch of (double or CSSNumericValue) is not a finite floating-point value.");
    seek = ms;
  }
  if (!SilentlySetCurrentTime(s, a, seek, "Animation.currentTime setter: ")) return false;
  if (Flag(a, kAPendingPause)) {
    SetOpt(a, kAHold, seek);
    ApplyPendingRate(a);
    SetOpt(a, kAStart, std::nullopt);
    CancelTask(a);
    if (!ResolveReady(s, a)) return false;
  }
  return UpdateFinished(s, a, true, false);
}

bool SetStartTime(State& s, JS::HandleObject a, JS::HandleValue value) {
  std::optional<double> start;
  if (!value.isNull()) {
    double ms = 0;
    if (!JS::ToNumber(s.cx, value, &ms)) return false;
    if (!std::isfinite(ms))
      return TypeFail(s.cx, "Animation.startTime setter: Double branch of (double or CSSNumericValue) is not a finite floating-point value.");
    start = ms;
  }
  if (!TimelineTime(TimelineOf(a)) && start) SetOpt(a, kAHold, std::nullopt);
  const auto previous = CurrentTime(a);
  ApplyPendingRate(a);
  SetOpt(a, kAStart, start);
  if (start) SetOpt(a, kAHold, Rate(a) != 0 ? std::nullopt : previous);
  else SetOpt(a, kAHold, previous);
  if (Pending(a)) {
    CancelTask(a);
    if (!ResolveReady(s, a)) return false;
  }
  return UpdateFinished(s, a, true, false);
}

bool Play(State& s, JS::HandleObject a, bool autoRewind, const char* prefix) {
  const bool abortedPause = Flag(a, kAPendingPause);
  bool pendingReady = false;
  std::optional<double> seek;
  const double rate = EffectiveRate(a), end = EndTime(a);
  const auto ct = CurrentTime(a);
  if (rate > 0 && autoRewind && (!ct || *ct < 0 || *ct >= end)) {
    seek = 0.0;
  } else if (rate < 0 && autoRewind && (!ct || *ct <= 0 || *ct > end)) {
    if (std::isinf(end))
      return DomFail(s, "InvalidStateError", std::string(prefix) + "Can't rewind animation with infinite effect end");
    seek = end;
  } else if (rate == 0 && !ct) {
    seek = 0.0;
  }
  if (seek) SetOpt(a, kAHold, seek);
  if (Opt(a, kAHold)) SetOpt(a, kAStart, std::nullopt);
  if (Pending(a)) { CancelTask(a); pendingReady = true; }
  if (!Opt(a, kAHold) && !seek && !abortedPause && JS::GetReservedSlot(a, kAPendingRate).isUndefined())
    return true;
  if (!pendingReady && !NewReady(s, a, false)) return false;
  SetFlag(a, kAPendingPlay, true);
  return ScheduleTask(s, a) && UpdateFinished(s, a, false, false);
}

bool Pause(State& s, JS::HandleObject a, const char* prefix) {
  if (Flag(a, kAPendingPause)) return true;
  std::optional<double> seek;
  if (!CurrentTime(a)) {
    if (Rate(a) >= 0) seek = 0.0;
    else if (std::isinf(EndTime(a)))
      return DomFail(s, "InvalidStateError", std::string(prefix) + "Can't pause animation with infinite effect end");
    else seek = EndTime(a);
  }
  if (seek) SetOpt(a, kAHold, seek);
  bool pendingReady = false;
  if (Flag(a, kAPendingPlay)) { CancelTask(a); pendingReady = true; }
  if (!pendingReady && !NewReady(s, a, false)) return false;
  SetFlag(a, kAPendingPause, true);
  return ScheduleTask(s, a) && UpdateFinished(s, a, false, false);
}

bool Finish(State& s, JS::HandleObject a) {
  const double rate = EffectiveRate(a), end = EndTime(a);
  if (rate == 0) return DomFail(s, "InvalidStateError", "Animation.finish: Can't finish animation with zero playback rate");
  if (rate > 0 && std::isinf(end))
    return DomFail(s, "InvalidStateError", "Animation.finish: Can't finish infinite animation");
  ApplyPendingRate(a);
  const double limit = rate > 0 ? end : 0;
  if (!SilentlySetCurrentTime(s, a, limit, "Animation.finish: ")) return false;
  const auto now = TimelineTime(TimelineOf(a));
  if (!Opt(a, kAStart) && now) SetOpt(a, kAStart, *now - limit / Rate(a));
  if (Flag(a, kAPendingPause) && Opt(a, kAStart)) {
    SetOpt(a, kAHold, std::nullopt);
    CancelTask(a);
    if (!ResolveReady(s, a)) return false;
  }
  if (Flag(a, kAPendingPlay) && Opt(a, kAStart)) {
    CancelTask(a);
    if (!ResolveReady(s, a)) return false;
  }
  return UpdateFinished(s, a, true, true);
}

bool Cancel(State& s, JS::HandleObject a) {
  if (std::strcmp(PlayState(a), "idle")) {
    if (Pending(a)) {
      CancelTask(a);
      ApplyPendingRate(a);
      if (!RejectSlot(s, a, kAReady) || !NewReady(s, a, true)) return false;
    }
    if (!RejectSlot(s, a, kAFinished) || !NewFinished(s, a) || !QueueEvent(s, a, "cancel", std::nullopt))
      return false;
  }
  SetOpt(a, kAHold, std::nullopt);
  SetOpt(a, kAStart, std::nullopt);
  JS::SetReservedSlot(a, kATimerTask, JS::NullValue());
  return true;
}

bool Reverse(State& s, JS::HandleObject a) {
  if (!TimelineTime(TimelineOf(a)))
    return DomFail(s, "InvalidStateError", "Animation.reverse: Can't reverse an animation with no associated timeline");
  JS::RootedValue original(s.cx, JS::GetReservedSlot(a, kAPendingRate));
  JS::SetReservedSlot(a, kAPendingRate, JS::DoubleValue(-EffectiveRate(a)));
  if (Play(s, a, true, "Animation.reverse: ")) return true;
  JS::SetReservedSlot(a, kAPendingRate, original);
  return false;
}

bool SetPlaybackRate(State& s, JS::HandleObject a, double rate) {
  JS::SetReservedSlot(a, kAPendingRate, JS::UndefinedValue());
  const auto previous = CurrentTime(a);
  JS::SetReservedSlot(a, kARate, JS::DoubleValue(rate));
  if (!previous) return true;
  JS::RootedValue value(s.cx, JS::DoubleValue(*previous));
  return SetCurrentTime(s, a, value);
}

bool UpdatePlaybackRate(State& s, JS::HandleObject a, double rate) {
  const std::string previous = PlayState(a);
  JS::SetReservedSlot(a, kAPendingRate, JS::DoubleValue(rate));
  if (Pending(a)) return true;  // The pending task applies it.
  if (previous == "idle" || previous == "paused") { ApplyPendingRate(a); return true; }
  if (previous == "finished") {
    const auto now = TimelineTime(TimelineOf(a));
    const auto start = Opt(a, kAStart);
    if (now && start) {
      const double unconstrained = (*now - *start) * Rate(a);
      SetOpt(a, kAStart, rate == 0 ? *now : *now - unconstrained / rate);
    }
    ApplyPendingRate(a);
    return UpdateFinished(s, a, false, false);
  }
  return Play(s, a, false, "Animation.updatePlaybackRate: ");
}
}  // namespace ruyijs::web_animation
