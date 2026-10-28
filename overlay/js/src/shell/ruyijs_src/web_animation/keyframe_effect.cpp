// AnimationEffect and KeyframeEffect (docs/owners/WEB_ANIMATION.md): both
// constructor overloads, the copy constructor, timing members and the
// target/pseudoElement/composite/iterationComposite attributes.
#include "web_animation/state.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::web_animation {
const JSClass kEffectClass = {"KeyframeEffect", JSCLASS_HAS_RESERVED_SLOTS(kECount)};
namespace {
enum Member { kGetTiming, kGetComputedTiming, kUpdateTiming, kGetKeyframes, kSetKeyframes,
              kTarget, kPseudo, kIterComposite, kComposite };
const char* const kMemberNames[] = {"getTiming", "getComputedTiming", "updateTiming", "getKeyframes",
                                    "setKeyframes", "target", "pseudoElement", "iterationComposite",
                                    "composite"};
JSObject* Effect(JS::HandleValue v) {
  JSObject* object = v.isObject() ? js::CheckedUnwrapStatic(&v.toObject()) : nullptr;
  return object && JS::GetClass(object) == &kEffectClass ? object : nullptr;
}
bool Enum(JSContext* cx, JS::HandleValue v, int first, int count, int* out, bool* valid) {
  std::string text;
  if (!Text(cx, v, &text)) return false;
  *valid = false;
  for (int i = first; i < first + count; ++i) if (text == kCompositeNames[i]) { *out = i; *valid = true; }
  return true;
}
bool EffectCreate(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  const std::string prefix = "KeyframeEffect constructor: ";
  if (!args.isConstructing()) return TypeFail(cx, prefix + "'new' is required");
  if (argc == 0) return TypeFail(cx, prefix + "0 is not a valid argument count for any overload.");
  JS::RootedObject proto(cx), effect(cx), target(cx);
  if (!s.kit.ConstructProto(args, s.keyframeEffect.prototype, &proto)) return false;
  if (argc == 1) {
    if (!args[0].isObject()) return TypeFail(cx, prefix + "Argument 1 is not an object.");
    JS::RootedObject source(cx, Effect(args[0]));
    if (!source) return TypeFail(cx, prefix + "Argument 1 does not implement interface KeyframeEffect.");
    effect = NewEffectObject(s, proto);
    if (!effect) return false;
    for (uint32_t slot = kETarget; slot < kEAnimation; ++slot)
      JS::SetReservedSlot(effect, slot, JS::GetReservedSlot(source, slot));
    args.rval().setObject(*effect);
    return trace.Finish(JS_WrapValue(cx, args.rval()));
  }
  if (!args[0].isNullOrUndefined()) {
    if (!args[0].isObject()) return TypeFail(cx, prefix + "Argument 1 is not an object.");
    if (s.dom.kind(s.dom.context, args[0]) != 1)
      return TypeFail(cx, prefix + "Argument 1 does not implement interface Element.");
    target = &args[0].toObject();
  }
  if (!args[1].isNullOrUndefined() && !args[1].isObject())
    return TypeFail(cx, prefix + "Argument 2 is not an object.");
  if (!NewKeyframeEffect(s, args, target, args[1], args.get(2), "EffectTiming", prefix, &effect)) return false;
  if (proto != s.keyframeEffect.prototype.get() && !JS_SetPrototype(cx, effect, proto)) return false;
  args.rval().setObject(*effect);
  return trace.Finish(true);
}
bool EffectMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int member = WebKit::Slot(args) % 100;
  const bool setter = WebKit::Slot(args) >= 100;
  const bool timing = member <= kUpdateTiming;
  JS::RootedObject self(cx, args.thisv().isObject() ? &args.thisv().toObject() : nullptr);
  if (!self || JS::GetClass(self) != &kEffectClass) {
    const std::string name = member >= kTarget ? std::string(setter ? "set " : "get ") + kMemberNames[member]
                                               : kMemberNames[member];
    return NotImplemented(cx, name.c_str(), timing ? "AnimationEffect" : "KeyframeEffect");
  }
  State& s = OwnerOf(self);
  JS::HandleValue v = args.get(0);
  int index = 0;
  bool valid = false;
  args.rval().setUndefined();
  switch (member) {
    case kGetTiming: case kGetComputedTiming:
      return TimingObject(cx, ReadTiming(self), member == kGetComputedTiming, self, args.rval());
    case kUpdateTiming: {
      model::EffectTiming t = ReadTiming(self);
      if (!ParseTiming(cx, v, true, "OptionalEffectTiming", "AnimationEffect.updateTiming: ", &t) ||
          !StoreTiming(self, t)) return false;
      JSObject* animation = JS::GetReservedSlot(self, kEAnimation).toObjectOrNull();
      JS::RootedObject owner(cx, animation);
      return !owner || UpdateFinished(s, owner, false, false);
    }
    case kGetKeyframes: {
      JS::RootedObject frames(cx, &JS::GetReservedSlot(self, kEFrames).toObject());
      return KeyframesObject(cx, frames, args.rval());
    }
    case kSetKeyframes: {
      JS::RootedObject frames(cx);
      if (!argc) return TypeFail(cx, "KeyframeEffect.setKeyframes: At least 1 argument required, but only 0 passed");
      if (!v.isNullOrUndefined() && !v.isObject())
        return TypeFail(cx, "KeyframeEffect.setKeyframes: Argument 1 is not an object.");
      if (!ParseKeyframes(cx, v, "KeyframeEffect.setKeyframes: ", &frames)) return false;
      JS::SetReservedSlot(self, kEFrames, JS::ObjectValue(*frames));
      return true;
    }
    case kTarget:
      if (!setter) { args.rval().set(JS::GetReservedSlot(self, kETarget)); return JS_WrapValue(cx, args.rval()); }
      if (!v.isNullOrUndefined() && !v.isObject())
        return TypeFail(cx, "KeyframeEffect.target setter: Value being assigned is not an object.");
      if (!v.isNullOrUndefined() && s.dom.kind(s.dom.context, v) != 1)
        return TypeFail(cx, "KeyframeEffect.target setter: Value being assigned does not implement interface Element.");
      JS::SetReservedSlot(self, kETarget, v.isObject() ? v : JS::NullValue());
      return true;
    case kPseudo: {
      if (!setter) { args.rval().set(JS::GetReservedSlot(self, kEPseudo)); return true; }
      JS::RootedValue canonical(cx);
      if (!PseudoElement(s, v, "KeyframeEffect.pseudoElement setter: ", &canonical)) return false;
      JS::SetReservedSlot(self, kEPseudo, canonical);
      return true;
    }
    default: {
      const uint32_t slot = member == kComposite ? kEComposite : kEIterComposite;
      if (!setter) return NewString(cx, kCompositeNames[JS::GetReservedSlot(self, slot).toInt32()], args.rval());
      // iterationComposite: replace|accumulate; composite: replace|add|accumulate.
      if (!Enum(cx, v, 0, 3, &index, &valid)) return false;
      if (valid && (member == kComposite || index != 1)) JS::SetReservedSlot(self, slot, JS::Int32Value(index));
      return true;
    }
  }
}
}  // namespace

bool InstallEffects(State& s) {
  JSContext* cx = s.cx;
  JS::RootedObject object(cx, JS::GetRealmObjectPrototype(cx));
  if (!s.kit.Define("AnimationEffect", &kEffectClass, object, WebKit::Reject, &s.effect) ||
      !s.kit.Method(s.effect, "AnimationEffect", "getTiming", EffectMember, 0, kGetTiming) ||
      !s.kit.Method(s.effect, "AnimationEffect", "getComputedTiming", EffectMember, 0, kGetComputedTiming) ||
      !s.kit.Method(s.effect, "AnimationEffect", "updateTiming", EffectMember, 0, kUpdateTiming) ||
      !s.kit.Define("KeyframeEffect", &kEffectClass, s.effect.prototype, EffectCreate, &s.keyframeEffect, 1) ||
      !s.kit.Method(s.keyframeEffect, "KeyframeEffect", "getKeyframes", EffectMember, 0, kGetKeyframes) ||
      !s.kit.Method(s.keyframeEffect, "KeyframeEffect", "setKeyframes", EffectMember, 1, kSetKeyframes))
    return false;
  // Getter slot m, setter slot m + 100, so one native tells them apart.
  for (int m = kTarget; m <= kComposite; ++m) {
    const std::string get = std::string("get ") + kMemberNames[m], set = std::string("set ") + kMemberNames[m];
    JS::RootedObject getter(cx, s.kit.Function(EffectMember, get.c_str(), 0, m));
    JS::RootedObject setter(cx, s.kit.Function(EffectMember, set.c_str(), 1, m + 100));
    JS::RootedObject proto(cx, s.keyframeEffect.prototype);
    if (!getter || !setter ||
        !JS_DefineProperty(cx, proto, kMemberNames[m], getter, setter, JSPROP_ENUMERATE) ||
        !s.kit.helpers->Accessor("KeyframeEffect", proto, kMemberNames[m]))
      return false;
  }
  return true;
}
}  // namespace ruyijs::web_animation
