// Canvas drawing resources: state lives only in these realm-owned instances.
#include <cmath>
#include "web_canvas/internal.h"
#include "js/Realm.h"

namespace ruyijs::web_canvas {
const JSClass kGradientClass = {"CanvasGradient", JSCLASS_HAS_RESERVED_SLOTS(1)};
const JSClass kPatternClass = {"CanvasPattern", JSCLASS_HAS_RESERVED_SLOTS(2)};
namespace {
bool ColorStop(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *detail::Self(args);
  JS::RootedObject self(cx);
  if (!WebKit::CheckBrand(cx, args, &kGradientClass, &self, "CanvasGradient")) return false;
  if (argc < 2) return WebKit::Fail(cx, "addColorStop requires offset and color");
  double offset;
  if (!JS::ToNumber(cx, args[0], &offset)) return false;
  JS::RootedString color(cx, JS::ToString(cx, args[1]));
  if (!color) return false;
  if (!std::isfinite(offset) || offset < 0 || offset > 1)
    return ThrowDOMException(s, "SyntaxError", "Invalid color stop offset");
  JS::RootedObject stops(cx, &JS::GetReservedSlot(self, 0).toObject());
  uint32_t n = 0;
  if (!JS::GetArrayLength(cx, stops, &n)) return false;
  args.rval().setUndefined();
  if (n >= 8192) return true;
  JS::RootedValue value(cx, JS::NumberValue(offset));
  if (!JS_SetElement(cx, stops, n, value)) return false;
  value.setString(color);
  return JS_SetElement(cx, stops, n + 1, value);
}
bool PatternTransform(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!WebKit::CheckBrand(cx, args, &kPatternClass, &self, "CanvasPattern")) return false;
  JS::RootedObject matrix(cx, JS::NewArrayObject(cx, 6));
  if (!matrix) return false;
  const char* names[] = {"a", "b", "c", "d", "e", "f"};
  for (int i = 0; i < 6; ++i) {
    JS::RootedValue v(cx);
    if (args.get(0).isObject()) {
      JS::RootedObject init(cx, &args[0].toObject());
      if (!JS_GetProperty(cx, init, names[i], &v)) return false;
    }
    double number = i == 0 || i == 3 ? 1 : 0;
    if (!v.isUndefined() && !JS::ToNumber(cx, v, &number)) return false;
    if (!std::isfinite(number)) { args.rval().setUndefined(); return true; }
    v.setNumber(number);
    if (!JS_SetElement(cx, matrix, i, v)) return false;
  }
  JS::SetReservedSlot(self, 1, JS::ObjectValue(*matrix));
  args.rval().setUndefined();
  return true;
}
}
bool CreateCanvasResource(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *detail::Self(args);
  int op = detail::ContextSlot(args);
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  if (op == 3) {
    if (argc < 2 || !args[0].isObject()) return WebKit::Fail(cx, "createPattern requires an image");
    JS::RootedString repeat(cx, JS::ToString(cx, args[1]));
    if (!repeat) return false;
    bool valid = args[1].isNull();
    for (const char* key : {"", "repeat", "repeat-x", "repeat-y", "no-repeat"}) {
      bool match = false;
      if (!detail::StringEqualsAscii(cx, repeat, key, &match)) return false;
      valid |= match;
    }
    if (!valid) return ThrowDOMException(s, "SyntaxError", "Invalid repetition");
    JS::RootedObject object(cx, s.kit.New(&kPatternClass, s.pattern.prototype));
    if (!object) return false;
    JS::SetReservedSlot(object, 0, args[0]);
    JS::SetReservedSlot(object, 1, JS::UndefinedValue());
    args.rval().setObject(*object);
    return true;
  }
  unsigned count = op == 0 ? 4 : op == 1 ? 6 : 3;
  if (argc < count) return WebKit::Fail(cx, "Missing gradient arguments");
  double values[6]; bool usable;
  if (!detail::ReadNumbers(cx, args, 0, count, values, &usable)) return false;
  if (!usable) return WebKit::Fail(cx, "Gradient arguments must be finite");
  if (op == 1 && (values[2] < 0 || values[5] < 0))
    return ThrowDOMException(s, "IndexSizeError", "Negative radius");
  JS::RootedObject object(cx, s.kit.New(&kGradientClass, s.gradient.prototype));
  JS::RootedObject stops(cx, JS::NewArrayObject(cx, 0));
  if (!object || !stops) return false;
  JS::SetReservedSlot(object, 0, JS::ObjectValue(*stops));
  args.rval().setObject(*object);
  return true;
}
bool InstallCanvasResources(State& s) {
  JS::RootedObject parent(s.cx, JS::GetRealmObjectPrototype(s.cx));
  return s.kit.Define("CanvasGradient", &kGradientClass, parent, WebKit::Reject, &s.gradient) &&
    s.kit.Method(s.gradient, "CanvasGradient", "addColorStop", ColorStop, 2, 0) &&
    s.kit.Define("CanvasPattern", &kPatternClass, parent, WebKit::Reject, &s.pattern) &&
    s.kit.Method(s.pattern, "CanvasPattern", "setTransform", PatternTransform, 0, 0);
}
}
