// KeyframeEffectOptions and the KeyframeEffect object factory
// (docs/owners/WEB_ANIMATION.md): timing, composite modes and the
// pseudo-element, converted and validated in Firefox's order.
#include "web_animation/state.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::web_animation {
// A Firefox-supported pseudo-element for animations, canonicalized; SyntaxError otherwise.
bool PseudoElement(State& s, JS::HandleValue v, const std::string& prefix, JS::MutableHandleValue out) {
  if (v.isNull()) { out.setNull(); return true; }
  std::string text, name;
  if (!Text(s.cx, v, &text)) return false;
  if (text.rfind("::", 0) == 0) name = text.substr(2);
  else if (text == ":before" || text == ":after" || text == ":first-line" || text == ":first-letter")
    name = text.substr(1);
  else if (!text.empty())
    return DomFail(s, "SyntaxError", prefix + "'" + text + "' is a syntactically invalid pseudo-element.");
  static const char* const known[] = {"before", "after", "marker", "first-line", "first-letter",
                                      "selection", "placeholder", "backdrop", "file-selector-button",
                                      "cue", "target-text", "details-content"};
  bool syntax = text.empty();
  for (const char* k : known) syntax = syntax || name == k;
  if (!syntax)
    return DomFail(s, "SyntaxError", prefix + "'" + text + "' is a syntactically invalid pseudo-element.");
  if (name != "before" && name != "after" && name != "marker")
    return DomFail(s, "SyntaxError", prefix + "'" + text + "' is an unsupported pseudo-element.");
  return NewString(s.cx, "::" + name, out);
}
JSObject* NewEffectObject(State& s, JS::HandleObject proto) {
  JSObject* e = s.kit.New(&kEffectClass, proto);
  if (!e) return nullptr;
  JS::SetReservedSlot(e, kEOwner, JS::PrivateValue(&s));
  for (uint32_t slot : {kETarget, kEPseudo, kEAnimation}) JS::SetReservedSlot(e, slot, JS::NullValue());
  JS::SetReservedSlot(e, kEComposite, JS::Int32Value(0));
  JS::SetReservedSlot(e, kEIterComposite, JS::Int32Value(0));
  return e;
}
bool NewKeyframeEffect(State& s, const JS::CallArgs&, JS::HandleObject target, JS::HandleValue keyframes,
                       JS::HandleValue options, const char* dictionary, const std::string& prefix,
                       JS::MutableHandleObject effect) {
  JSContext* cx = s.cx;
  model::EffectTiming timing;
  JS::RootedValue v(cx), pseudo(cx, JS::NullValue());
  int composite = 0, iterComposite = 0;
  if (options.isObject()) {
    JS::RootedObject o(cx, &options.toObject());
    // KeyframeEffectOptions own members, lexicographic, after the EffectTiming members.
    const auto readEnum = [&](const char* member, const char* enumeration, bool allowAdd, int* out) {
      if (!JS_GetProperty(cx, o, member, &v)) return false;
      if (v.isUndefined()) return true;
      std::string text;
      if (!Text(cx, v, &text)) return false;
      for (int i = 0; i < 3; ++i)
        if (text == kCompositeNames[i] && (allowAdd || i != 1)) { *out = i; return true; }
      return TypeFail(cx, prefix + "'" + text + "' (value of '" + member + "' member of KeyframeEffectOptions) "
                          "is not a valid value for enumeration " + enumeration + ".");
    };
    if (!ParseTiming(cx, options, false, dictionary, prefix, &timing) ||
        !readEnum("composite", "CompositeOperation", true, &composite) ||
        !readEnum("iterationComposite", "IterationCompositeOperation", false, &iterComposite) ||
        !JS_GetProperty(cx, o, "pseudoElement", &v) ||
        (!v.isUndefined() && !PseudoElement(s, v, prefix, &pseudo)))
      return false;
  } else if (!ParseTiming(cx, options, false, dictionary, prefix, &timing)) {
    return false;
  }
  JS::RootedObject frames(cx), proto(cx, s.keyframeEffect.prototype);
  if (!ParseKeyframes(cx, keyframes, prefix, &frames)) return false;
  effect.set(NewEffectObject(s, proto));
  if (!effect || !StoreTiming(effect, timing)) return false;
  JS::SetReservedSlot(effect, kETarget, target ? JS::ObjectValue(*target) : JS::NullValue());
  JS::SetReservedSlot(effect, kEPseudo, pseudo);
  JS::SetReservedSlot(effect, kEComposite, JS::Int32Value(composite));
  JS::SetReservedSlot(effect, kEIterComposite, JS::Int32Value(iterComposite));
  JS::SetReservedSlot(effect, kEFrames, JS::ObjectValue(*frames));
  return true;
}

}  // namespace ruyijs::web_animation
