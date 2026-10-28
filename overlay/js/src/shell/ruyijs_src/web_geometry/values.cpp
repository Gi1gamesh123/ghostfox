#include "web_geometry/internal.h"
#include "domain/geometry/values.h"
#include "runtime/native_constructor_trace.h"
#include "js/Wrapper.h"

namespace ruyijs::web_geometry {
const JSClass kRectReadOnlyClass = {"DOMRectReadOnly", JSCLASS_HAS_RESERVED_SLOTS(4), nullptr};
const JSClass kRectClass = {"DOMRect", JSCLASS_HAS_RESERVED_SLOTS(4), nullptr};
const JSClass kPointReadOnlyClass = {"DOMPointReadOnly", JSCLASS_HAS_RESERVED_SLOTS(4), nullptr};
const JSClass kPointClass = {"DOMPoint", JSCLASS_HAS_RESERVED_SLOTS(4), nullptr};
const JSClass kQuadClass = {"DOMQuad", JSCLASS_HAS_RESERVED_SLOTS(8), nullptr};

bool Receiver(JSContext* cx, JS::HandleValue value, Kind kind, JS::MutableHandleObject out) {
  out.set(value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  const JSClass* clasp = out ? JS::GetClass(out) : nullptr;
  bool ok = kind == RectReadOnly ? clasp == &kRectReadOnlyClass || clasp == &kRectClass :
            kind == PointReadOnly ? clasp == &kPointReadOnlyClass || clasp == &kPointClass :
            kind == Rect ? clasp == &kRectClass : kind == Point ? clasp == &kPointClass : clasp == &kQuadClass;
  return ok || WebKit::Fail(cx, "Geometry member called on an incompatible receiver");
}
bool NewValueObject(JSContext* cx, const JSClass* clasp, JS::HandleObject proto,
                    const double* values, unsigned count, JS::MutableHandleValue result) {
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, clasp, proto));
  if (!object) return false;
  for (unsigned i = 0; i < count; ++i)
    JS::SetReservedSlot(object, i, JS::NumberValue(values[i]));
  result.setObject(*object);
  return true;
}
bool NewValue(State& s, Kind kind, const double* values, const JS::CallArgs* args,
              JS::MutableHandleValue result) {
  WebFace& face = s.faces[kind];
  JS::RootedObject proto(s.cx, face.prototype);
  if (args && !s.kit.ConstructProto(*args, face.prototype, &proto)) return false;
  return NewValueObject(s.cx, face.clasp, proto, values, kind == Quad ? 8u : 4u, result);
}
bool ReadDictionary(State& s, JS::HandleValue input, const char* const* names,
                     unsigned count, double* values, bool strict) {
  if (input.isUndefined()) return true;
  if (!input.isObject())
    return !strict || WebKit::Fail(s.cx, "Geometry dictionary must be an object");
  JS::RootedObject object(s.cx, &input.toObject());
  for (unsigned i = 0; i < count; ++i) {
    JS::RootedValue value(s.cx);
    if (!JS_GetProperty(s.cx, object, names[i], &value)) return false;
    if (strict) {
      if (!value.isUndefined() && !JS::ToNumber(s.cx, value, &values[i])) return false;
    } else if (value.isNumber()) values[i] = value.toNumber();
  }
  return true;
}
bool PlainValues(State& s, const char* const* names, const double* values,
                  unsigned count, JS::MutableHandleValue result) {
  JS::RootedObject object(s.cx, JS_NewPlainObject(s.cx));
  if (!object) return false;
  for (unsigned i = 0; i < count; ++i)
    if (!JS_DefineProperty(s.cx, object, names[i], values[i], JSPROP_ENUMERATE)) return false;
  result.setObject(*object);
  return true;
}
bool RectSnapshot(State& s, const double* rect, JS::MutableHandleValue result) {
  double values[8] = {rect[0], rect[1], rect[2], rect[3]};
  for (unsigned i = 0; i < 4; ++i) values[i + 4] = domain::geometry::RectSide(rect, i);
  return PlainValues(s, kRectFields, values, 8, result);
}
bool Construct(JSContext* cx, unsigned argc, JS::Value* vp, Kind kind) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  NativeConstructorTrace trace(cx, args);
  if (!args.isConstructing()) return WebKit::Fail(cx, "Geometry constructor requires new");
  if (kind == Quad) return trace.Finish(ConstructQuad(s, args));
  double values[4] = {0, 0, 0, kind >= PointReadOnly ? 1.0 : 0.0};
  for (unsigned i = 0; i < 4; ++i)
    if (!args.get(i).isUndefined() && !JS::ToNumber(cx, args.get(i), &values[i])) return false;
  return trace.Finish(NewValue(s, kind, values, &args, args.rval()));
}
}
