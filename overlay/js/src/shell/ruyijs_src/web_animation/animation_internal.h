#ifndef RUYIJS_WEB_ANIMATION_ANIMATION_INTERNAL_H
#define RUYIJS_WEB_ANIMATION_ANIMATION_INTERNAL_H
// Helpers shared by animation.cpp, animation_state.cpp and animation_play.cpp.
#include "web_animation/state.h"

namespace ruyijs::web_animation {
inline std::optional<double> Opt(JSObject* a, uint32_t slot) {
  const JS::Value v = JS::GetReservedSlot(a, slot);
  if (v.isNumber()) return v.toNumber();
  return std::nullopt;
}
inline void SetOpt(JSObject* a, uint32_t slot, std::optional<double> v) {
  JS::SetReservedSlot(a, slot, v ? JS::DoubleValue(*v) : JS::NullValue());
}
inline bool Flag(JSObject* a, uint32_t slot) { return JS::GetReservedSlot(a, slot).toBoolean(); }
inline void SetFlag(JSObject* a, uint32_t slot, bool v) { JS::SetReservedSlot(a, slot, JS::BooleanValue(v)); }
inline double Rate(JSObject* a) { return JS::GetReservedSlot(a, kARate).toDouble(); }
inline double EffectiveRate(JSObject* a) {
  const JS::Value pending = JS::GetReservedSlot(a, kAPendingRate);
  return pending.isUndefined() ? Rate(a) : pending.toDouble();
}
inline void ApplyPendingRate(JSObject* a) {
  JS::SetReservedSlot(a, kARate, JS::DoubleValue(EffectiveRate(a)));
  JS::SetReservedSlot(a, kAPendingRate, JS::UndefinedValue());
}
inline bool Pending(JSObject* a) { return Flag(a, kAPendingPlay) || Flag(a, kAPendingPause); }
inline JSObject* EffectOf(JSObject* a) { return JS::GetReservedSlot(a, kAEffect).toObjectOrNull(); }
inline JSObject* TimelineOf(JSObject* a) { return JS::GetReservedSlot(a, kATimeline).toObjectOrNull(); }
inline double EndTime(JSObject* a) {
  JSObject* effect = EffectOf(a);
  return effect ? ReadTiming(effect).EndTime() : 0;
}
// A promise of the animation's realm; ready/finished bookkeeping.
bool NewReady(State& s, JS::HandleObject animation, bool resolved);
bool ResolveReady(State& s, JS::HandleObject animation);
bool RejectSlot(State& s, JS::HandleObject animation, uint32_t slot);  // AbortError, marked handled
bool NewFinished(State& s, JS::HandleObject animation);
// The pending play/pause task; one at a time, replaced by identity.
bool ScheduleTask(State& s, JS::HandleObject animation);
inline void CancelTask(JSObject* a) {
  SetFlag(a, kAPendingPlay, false);
  SetFlag(a, kAPendingPause, false);
  JS::SetReservedSlot(a, kAPendingTask, JS::NullValue());
}
// Adds a non-idle animation to the realm list getAnimations() reads (animation_list.cpp).
bool Track(State& s, JSObject* animation);
bool QueueEvent(State& s, JS::HandleObject animation, const char* type, std::optional<double> currentTime);
bool SilentlySetCurrentTime(State& s, JS::HandleObject animation, std::optional<double> seek, const char* prefix);
}  // namespace ruyijs::web_animation
#endif
