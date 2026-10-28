// The Animation object factory and "set the associated effect"
// (web-animations-1 §4.4.2): an effect belongs to at most one animation.
#include "web_animation/animation_internal.h"
#include "interaction/realm.h"
#include "js/Object.h"

namespace ruyijs::web_animation {
bool NewAnimation(State& s, JS::HandleObject proto, JS::HandleObject effect, JS::HandleObject timeline,
                  JS::MutableHandleObject a) {
  a.set(s.kit.New(&kAnimationClass, proto));
  if (!a || !s.events.Attach(a)) return false;
  JS::SetReservedSlot(a, kAOwner, JS::PrivateValue(&s));
  JS::SetReservedSlot(a, kAId, JS::StringValue(JS_GetEmptyString(s.cx)));
  for (uint32_t slot : {kAEffect, kAStart, kAHold, kAPendingTask, kATimerTask, kAPrevious, kAOnFinish,
                        kAOnCancel, kAOnRemove})
    JS::SetReservedSlot(a, slot, JS::NullValue());
  JS::SetReservedSlot(a, kATimeline, timeline ? JS::ObjectValue(*timeline) : JS::NullValue());
  JS::SetReservedSlot(a, kARate, JS::DoubleValue(1));
  JS::SetReservedSlot(a, kAPendingRate, JS::UndefinedValue());
  SetFlag(a, kAPendingPlay, false);
  SetFlag(a, kAPendingPause, false);
  JS::SetReservedSlot(a, kAReplace, JS::Int32Value(0));
  JS::SetReservedSlot(a, kASequence, JS::Int32Value(s.sequence++));
  return NewReady(s, a, true) && NewFinished(s, a) && SetEffect(s, a, effect);
}

bool SetEffect(State& s, JS::HandleObject a, JS::HandleObject effect) {
  JS::RootedObject old(s.cx, EffectOf(a));
  if (old == effect) return true;
  if (effect) {
    JS::RootedObject other(s.cx, JS::GetReservedSlot(effect, kEAnimation).toObjectOrNull());
    if (other && other != a) {
      JS::SetReservedSlot(other, kAEffect, JS::NullValue());
      if (!UpdateFinished(OwnerOf(other), other, false, false)) return false;
    }
    JS::SetReservedSlot(effect, kEAnimation, JS::ObjectValue(*a));
  }
  if (old) JS::SetReservedSlot(old, kEAnimation, JS::NullValue());
  JS::SetReservedSlot(a, kAEffect, effect ? JS::ObjectValue(*effect) : JS::NullValue());
  return UpdateFinished(s, a, false, false);
}

}  // namespace ruyijs::web_animation
