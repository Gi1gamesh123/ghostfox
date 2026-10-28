// Element.animate and Element.getAnimations (the Animatable mixin), installed
// once by this owner on the Document Model's Element prototype.
#include "web_animation/animation_internal.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::web_animation {
namespace {
bool Animate(JSContext* cx, const JS::CallArgs& args, State& s) {
  const std::string prefix = "Element.animate: ";
  if (!args.length()) return TypeFail(cx, prefix + "At least 1 argument required, but only 0 passed");
  if (!args[0].isNullOrUndefined() && !args[0].isObject())
    return TypeFail(cx, prefix + "Argument 1 is not an object.");
  JS::RootedObject target(cx, &args.thisv().toObject()), effect(cx), timeline(cx), animation(cx);
  JS::HandleValue options = args.get(1);
  // KeyframeAnimationOptions own member id; Firefox 158 does not read `timeline`
  // (the animation joins the target's document timeline).
  JS::RootedValue id(cx);
  if (!NewKeyframeEffect(s, args, target, args[0], options, "EffectTiming", prefix, &effect)) return false;
  if (options.isObject()) {
    JS::RootedObject o(cx, &options.toObject());
    if (!JS_GetProperty(cx, o, "id", &id)) return false;
  }
  JS::RootedObject document(cx);
  if (!s.dom.document(s.dom.context, target, &document) || !DocumentTimeline(s, document, &timeline))
    return false;
  JS::RootedObject proto(cx, s.animation.prototype);
  if (!NewAnimation(s, proto, effect, timeline, &animation)) return false;
  if (!id.isUndefined()) {
    JS::RootedValue text(cx);
    std::string value;
    if (!Text(cx, id, &value) || !NewString(cx, value, &text)) return false;
    JS::SetReservedSlot(animation, kAId, text);
  }
  if (!Play(s, animation, true, "Element.animate: ")) return false;
  args.rval().setObject(*animation);
  return JS_WrapValue(cx, args.rval());
}
bool ElementMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  const bool animate = WebKit::Slot(args) == 0;
  if (s.dom.kind(s.dom.context, args.thisv()) != 1)
    return NotImplemented(cx, animate ? "animate" : "getAnimations", "Element");
  JSAutoRealm realm(cx, s.kit.page);
  if (animate) return Animate(cx, args, s);
  bool subtree = false;
  JS::HandleValue options = args.get(0);
  if (!options.isNullOrUndefined()) {
    if (!options.isObject())
      return TypeFail(cx, "Element.getAnimations: Argument 1 can't be converted to a dictionary.");
    JS::RootedObject o(cx, &options.toObject());
    JS::RootedValue v(cx);
    if (!JS_GetProperty(cx, o, "subtree", &v)) return false;
    subtree = JS::ToBoolean(v);
  }
  JS::RootedObject root(cx, &args.thisv().toObject());
  return AnimationsFor(s, root, subtree, args.rval());
}
}  // namespace

bool InstallElementMembers(State& s, JS::HandleObject elementPrototype) {
  WebFace element(s.cx);
  element.prototype = elementPrototype;
  return s.kit.Method(element, "Element", "animate", ElementMember, 1, 0) &&
         s.kit.Method(element, "Element", "getAnimations", ElementMember, 0, 1);
}
}  // namespace ruyijs::web_animation
