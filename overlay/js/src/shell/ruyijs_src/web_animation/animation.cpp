// The Animation interface (EventTarget), CSSAnimation and CSSTransition
// (docs/owners/WEB_ANIMATION.md): constructor, attributes in Firefox's order,
// methods and the three event handler attributes.
#include "web_animation/animation_internal.h"
#include "interaction/realm.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace ruyijs::web_animation {
const JSClass kAnimationClass = {"Animation", JSCLASS_HAS_RESERVED_SLOTS(kACount)};
const JSClass kCssAnimationClass = {"CSSAnimation", JSCLASS_HAS_RESERVED_SLOTS(kACount)};
const JSClass kCssTransitionClass = {"CSSTransition", JSCLASS_HAS_RESERVED_SLOTS(kACount)};
namespace {
enum Attribute { aId, aEffect, aTimeline, aStart, aCurrent, aOverall, aRate, aPlayState, aPending,
                 aReplace, aReady, aFinished, aOnFinish, aOnCancel, aOnRemove, aCount };
const char* const kAttributes[] = {"id", "effect", "timeline", "startTime", "currentTime", "overallProgress",
                                   "playbackRate", "playState", "pending", "replaceState", "ready",
                                   "finished", "onfinish", "oncancel", "onremove"};
const bool kWritable[] = {true, true, true, true, true, false, true, false, false, false, false, false,
                          true, true, true};
const char* const kMethods[] = {"cancel", "finish", "play", "pause", "updatePlaybackRate", "reverse",
                                "persist", "commitStyles"};
const char* const kReplace[] = {"active", "removed", "persisted"};
JSObject* Unwrapped(JS::HandleValue v) {
  return v.isObject() ? js::CheckedUnwrapStatic(&v.toObject()) : nullptr;
}
bool Finite(JSContext* cx, JS::HandleValue v, const std::string& message, double* out) {
  if (!JS::ToNumber(cx, v, out)) return false;
  return std::isfinite(*out) || TypeFail(cx, message);
}
bool AnimationCreate(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  if (!args.isConstructing()) return TypeFail(cx, "Animation constructor: 'new' is required");
  JS::RootedObject effect(cx), timeline(cx), proto(cx), animation(cx);
  if (!args.get(0).isNullOrUndefined()) {
    effect = Unwrapped(args[0]);
    if (!effect || JS::GetClass(effect) != &kEffectClass)
      return TypeFail(cx, "Animation constructor: Argument 1 does not implement interface AnimationEffect.");
  }
  if (args.get(1).isUndefined()) {
    JS::RootedObject document(cx, s.dom.mainDocument(s.dom.context));
    if (document && !DocumentTimeline(s, document, &timeline)) return false;
  } else if (!args[1].isNull()) {
    timeline = Unwrapped(args[1]);
    if (!timeline || JS::GetClass(timeline) != &kTimelineClass)
      return TypeFail(cx, "Animation constructor: Argument 2 does not implement interface AnimationTimeline.");
  }
  if (!s.kit.ConstructProto(args, s.animation.prototype, &proto) ||
      !NewAnimation(s, proto, effect, timeline, &animation)) return false;
  args.rval().setObject(*animation);
  return trace.Finish(true);
}
bool Handler(State& s, JS::HandleObject self, int which, bool set, JS::HandleValue v, JS::MutableHandleValue rval) {
  const uint32_t slot = kAOnFinish + which;
  if (!set) { rval.set(JS::GetReservedSlot(self, slot)); return JS_WrapValue(s.cx, rval); }
  JS::RootedValue previous(s.cx, JS::GetReservedSlot(self, slot)), desired(s.cx, v);
  if (!desired.isObject() || !JS::IsCallable(&desired.toObject())) desired.setNull();
  JS::RootedObject prototype(s.cx, s.events.prototype());
  JS::RootedValue function(s.cx), receiver(s.cx, JS::ObjectValue(*self)), ignored(s.cx);
  JS::RootedValueArray<2> parameters(s.cx);
  if (!NewString(s.cx, kHandlerNames[which], parameters[0])) return false;
  for (int add = 0; add < 2; ++add) {
    parameters[1].set(add ? desired : previous);
    if (!parameters[1].isObject()) continue;
    if (!JS_GetProperty(s.cx, prototype, add ? "addEventListener" : "removeEventListener", &function) ||
        !JS::Call(s.cx, receiver, function, parameters, &ignored)) return false;
  }
  JS::SetReservedSlot(self, slot, desired);
  rval.setUndefined();
  return true;
}
bool Member(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int slot = WebKit::Slot(args);
  const bool method = slot >= 200, set = slot >= 100 && !method;
  const int index = method ? slot - 200 : slot % 100;
  JS::RootedObject self(cx, args.thisv().isObject() ? &args.thisv().toObject() : nullptr);
  if (!IsAnimation(self)) {
    const std::string name = method ? kMethods[index] : std::string(set ? "set " : "get ") + kAttributes[index];
    NotImplemented(cx, name.c_str(), "Animation");
    if (method || (index != aReady && index != aFinished)) return false;
    return RejectPending(*static_cast<State*>(WebKit::Owner(args)), args.rval());
  }
  State& s = OwnerOf(self);
  JSAutoRealm realm(cx, self);
  JS::HandleValue v = args.get(0);
  double number = 0;
  if (method) {
    args.rval().setUndefined();
    switch (index) {
      case 0: return Cancel(s, self);
      case 1: return Finish(s, self);
      case 2: return Play(s, self, true, "Animation.play: ");
      case 3: return Pause(s, self, "Animation.pause: ");
      case 4:
        if (!argc) return TypeFail(cx, "Animation.updatePlaybackRate: At least 1 argument required, but only 0 passed");
        return Finite(cx, v, "Animation.updatePlaybackRate: Argument 1 is not a finite floating-point value.", &number) &&
               UpdatePlaybackRate(s, self, number);
      case 5: return Reverse(s, self);
      case 6: JS::SetReservedSlot(self, kAReplace, JS::Int32Value(2)); return true;
      default: return true;  // commitStyles: effects apply no style here (recorded deviation)
    }
  }
  if (index >= aOnFinish) return Handler(s, self, index - aOnFinish, set, v, args.rval());
  JS::MutableHandleValue rval = args.rval();
  const auto opt = [&](std::optional<double> d) { if (d) rval.setDouble(*d); else rval.setNull(); return true; };
  if (set) {
    rval.setUndefined();
    switch (index) {
      case aId: { std::string id; return Text(cx, v, &id) && NewString(cx, id, rval) &&
                    (JS::SetReservedSlot(self, kAId, rval), rval.setUndefined(), true); }
      case aEffect: {
        JS::RootedObject effect(cx, v.isNullOrUndefined() ? nullptr : Unwrapped(v));
        if (!v.isNullOrUndefined() && !v.isObject())
          return TypeFail(cx, "Animation.effect setter: Value being assigned is not an object.");
        if (!v.isNullOrUndefined() && (!effect || JS::GetClass(effect) != &kEffectClass))
          return TypeFail(cx, "Animation.effect setter: Value being assigned does not implement interface AnimationEffect.");
        return SetEffect(s, self, effect);
      }
      case aTimeline: {
        JS::RootedObject timeline(cx, v.isNullOrUndefined() ? nullptr : Unwrapped(v));
        if (!v.isNullOrUndefined() && (!timeline || JS::GetClass(timeline) != &kTimelineClass))
          return TypeFail(cx, "Animation.timeline setter: Value being assigned does not implement interface AnimationTimeline.");
        if (timeline == TimelineOf(self)) return true;
        JS::SetReservedSlot(self, kATimeline, timeline ? JS::ObjectValue(*timeline) : JS::NullValue());
        if (Opt(self, kAStart)) SetOpt(self, kAHold, std::nullopt);
        return UpdateFinished(s, self, false, false);
      }
      case aStart: return SetStartTime(s, self, v);
      case aCurrent: return SetCurrentTime(s, self, v);
      default:
        return Finite(cx, v, "Animation.playbackRate setter: Value being assigned is not a finite floating-point value.",
                      &number) && SetPlaybackRate(s, self, number);
    }
  }
  switch (index) {
    case aId: rval.set(JS::GetReservedSlot(self, kAId)); return true;
    case aEffect: rval.set(JS::GetReservedSlot(self, kAEffect)); return JS_WrapValue(cx, rval);
    case aTimeline: rval.set(JS::GetReservedSlot(self, kATimeline)); return JS_WrapValue(cx, rval);
    case aStart: return opt(Opt(self, kAStart));
    case aCurrent: return opt(CurrentTime(self));
    case aOverall: {
      const auto ct = CurrentTime(self);
      if (!EffectOf(self) || !ct) return opt(std::nullopt);
      const double end = EndTime(self);
      return opt(end == 0 ? (*ct < 0 ? 0 : 1) : std::min(std::max(*ct / end, 0.0), 1.0));
    }
    case aRate: rval.setDouble(Rate(self)); return true;
    case aPlayState: return NewString(cx, PlayState(self), rval);
    case aPending: rval.setBoolean(Pending(self)); return true;
    case aReplace: return NewString(cx, kReplace[JS::GetReservedSlot(self, kAReplace).toInt32()], rval);
    default:
      rval.set(JS::GetReservedSlot(self, index == aReady ? kAReady : kAFinished));
      return JS_WrapValue(cx, rval);
  }
}
bool CssName(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const bool animation = WebKit::Slot(args) == 0;
  JSObject* self = args.thisv().isObject() ? &args.thisv().toObject() : nullptr;
  if (!self || JS::GetClass(self) != (animation ? &kCssAnimationClass : &kCssTransitionClass))
    return NotImplemented(cx, animation ? "get animationName" : "get transitionProperty",
                          animation ? "CSSAnimation" : "CSSTransition");
  args.rval().setString(JS_GetEmptyString(cx));  // No CSS-created instance exists (recorded deviation).
  return true;
}
}  // namespace

bool InstallAnimations(State& s) {
  JSContext* cx = s.cx;
  JS::RootedObject target(cx, s.events.prototype());
  if (!target || !s.kit.Define("Animation", &kAnimationClass, target, AnimationCreate, &s.animation)) return false;
  for (int i = 0; i < 8; ++i)
    if (!s.kit.Method(s.animation, "Animation", kMethods[i], Member, i == 4 ? 1 : 0, 200 + i)) return false;
  JS::RootedObject proto(cx, s.animation.prototype);
  for (int i = 0; i < aCount; ++i) {
    const std::string get = std::string("get ") + kAttributes[i], set = std::string("set ") + kAttributes[i];
    JS::RootedObject getter(cx, s.kit.Function(Member, get.c_str(), 0, i)), setter(cx);
    if (kWritable[i]) setter = s.kit.Function(Member, set.c_str(), 1, 100 + i);
    if (!getter || (kWritable[i] && !setter) ||
        !JS_DefineProperty(cx, proto, kAttributes[i], getter, setter, JSPROP_ENUMERATE) ||
        !s.kit.helpers->Accessor("Animation", proto, kAttributes[i])) return false;
  }
  return s.kit.Define("CSSAnimation", &kCssAnimationClass, proto, WebKit::Reject, &s.cssAnimation) &&
         s.kit.Accessor(s.cssAnimation, "CSSAnimation", "animationName", CssName, nullptr, 0) &&
         s.kit.Define("CSSTransition", &kCssTransitionClass, proto, WebKit::Reject, &s.cssTransition) &&
         s.kit.Accessor(s.cssTransition, "CSSTransition", "transitionProperty", CssName, nullptr, 1);
}
}  // namespace ruyijs::web_animation
