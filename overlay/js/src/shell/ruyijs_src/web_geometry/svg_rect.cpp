#include "web_geometry/svg.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include <cmath>

namespace ruyijs::web_geometry {
namespace {
const JSClass kSvgRectClass = {"SVGRect", JSCLASS_HAS_RESERVED_SLOTS(6)};
bool RectReceiver(JSContext* cx, JS::HandleValue value, JS::MutableHandleObject out, unsigned slot, bool setter) {
  out.set(value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  if (out && JS::GetClass(out) == &kSvgRectClass) return true;
  static const char* getErrors[] = {
    "'get x' called on an object that does not implement interface SVGRect.",
    "'get y' called on an object that does not implement interface SVGRect.",
    "'get width' called on an object that does not implement interface SVGRect.",
    "'get height' called on an object that does not implement interface SVGRect."};
  static const char* setErrors[] = {
    "'set x' called on an object that does not implement interface SVGRect.",
    "'set y' called on an object that does not implement interface SVGRect.",
    "'set width' called on an object that does not implement interface SVGRect.",
    "'set height' called on an object that does not implement interface SVGRect."};
  return WebKit::Fail(cx, setter ? setErrors[slot] : getErrors[slot]);
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto slot = WebKit::Slot(args);
  JS::RootedObject object(cx);
  if (!RectReceiver(cx, args.thisv(), &object, slot, false)) return false;
  if (!JS::GetReservedSlot(object, 5).toInt32()) {
    args.rval().set(JS::GetReservedSlot(object, slot)); return true;
  }
  State& s = Self(args);
  JS::RootedValue node(cx, JS::GetReservedSlot(object, 4));
  if (!JS_WrapValue(cx, &node)) return false;
  bool supported = false; document_model::SvgViewBoxValue value; void* factory = nullptr;
  if (!s.svgPorts.viewBoxRead || !s.svgPorts.viewBoxRead(s.svgPorts.context, node, &supported, &value, &factory))
    return false;
  args.rval().setDouble(value.fields[slot]); return true;
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto slot = WebKit::Slot(args);
  JS::RootedObject object(cx);
  if (!RectReceiver(cx, args.thisv(), &object, slot, true)) return false;
  double value;
  if (!JS::ToNumber(cx, args.get(0), &value)) return false;
  const float number = float(value);
  static const char* finiteErrors[] = {
    "SVGRect.x setter: Value being assigned is not a finite floating-point value.",
    "SVGRect.y setter: Value being assigned is not a finite floating-point value.",
    "SVGRect.width setter: Value being assigned is not a finite floating-point value.",
    "SVGRect.height setter: Value being assigned is not a finite floating-point value."};
  if (!std::isfinite(number)) return WebKit::Fail(cx, finiteErrors[slot]);
  const unsigned mode = JS::GetReservedSlot(object, 5).toInt32();
  if (mode == 2) {
    State& s = Self(args);
    static const char* readonlyErrors[] = {
      "SVGRect.x setter: Animated values cannot be set",
      "SVGRect.y setter: Animated values cannot be set",
      "SVGRect.width setter: Animated values cannot be set",
      "SVGRect.height setter: Animated values cannot be set"};
    return s.exception ? s.exception(s.exceptionContext, "NoModificationAllowedError", readonlyErrors[slot])
                       : WebKit::Fail(cx, readonlyErrors[slot]);
  }
  if (mode == 1) {
    State& s = Self(args);
    JS::RootedValue node(cx, JS::GetReservedSlot(object, 4));
    if (!JS_WrapValue(cx, &node) || !s.svgPorts.viewBoxWrite ||
        !s.svgPorts.viewBoxWrite(s.svgPorts.context, node, slot, number)) return false;
  } else JS::SetReservedSlot(object, slot, JS::DoubleValue(number));
  args.rval().setUndefined(); return true;
}
}
bool InstallSvgRect(State& s) {
  JS::RootedObject plain(s.cx, JS::GetRealmObjectPrototype(s.cx));
  if (!s.kit.Define("SVGRect", &kSvgRectClass, plain, WebKit::Reject, &s.svgRect) ||
      !JS_DeleteProperty(s.cx, s.svgRect.prototype, "constructor")) return false;
  const char* names[] = {"x", "y", "width", "height"};
  for (unsigned i = 0; i < 4; ++i)
    if (!s.kit.Accessor(s.svgRect, "SVGRect", names[i], Get, Set, i)) return false;
  return JS_DefineProperty(s.cx, s.svgRect.prototype, "constructor", s.svgRect.constructor, 0);
}
// The same native rectangle class serves detached snapshots and live viewBox views.
// Modes 1/2 store no numeric copy; their single node edge is the value source.
bool NewSvgRectObject(State& s, const float* values, JS::HandleObject parent, unsigned mode,
                      JS::MutableHandleObject result) {
  JS::RootedObject object(s.cx, JS_NewObjectWithGivenProto(s.cx, &kSvgRectClass, s.svgRect.prototype));
  if (!object) return false;
  if (!mode) for (unsigned i = 0; i < 4; ++i) JS::SetReservedSlot(object, i, JS::DoubleValue(values[i]));
  JS::SetReservedSlot(object, 4, JS::ObjectOrNullValue(parent));
  JS::SetReservedSlot(object, 5, JS::Int32Value(mode));
  result.set(object); return true;
}
bool Realm::NewSvgRect(const float* values, JS::HandleObject parent, JS::MutableHandleValue result) {
  State& s = *state_;
  {
    JSAutoRealm realm(s.cx, s.kit.page);
    JS::RootedObject edge(s.cx, parent);
    if (!JS_WrapObject(s.cx, &edge)) return false;
    JS::RootedObject object(s.cx);
    if (!NewSvgRectObject(s, values, edge, 0, &object)) return false;
    result.setObject(*object);
  }
  return JS_WrapValue(s.cx, result);
}
}
