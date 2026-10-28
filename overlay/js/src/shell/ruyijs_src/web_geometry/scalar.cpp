#include "web_geometry/internal.h"
#include "domain/geometry/values.h"
#include "js/Realm.h"

namespace ruyijs::web_geometry {
bool ScalarGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  int slot = WebKit::Slot(args);
  JS::RootedObject object(cx);
  if (!Receiver(cx, args.thisv(), Kind(slot / 16), &object)) return false;
  unsigned field = slot % 16;
  if (field < 4) args.rval().set(JS::GetReservedSlot(object, field));
  else {
    double values[4];
    for (unsigned i = 0; i < 4; ++i) values[i] = JS::GetReservedSlot(object, i).toNumber();
    args.rval().setNumber(domain::geometry::RectSide(values, field - 4));
  }
  return true;
}
bool ScalarSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  int slot = WebKit::Slot(args);
  JS::RootedObject object(cx);
  if (!Receiver(cx, args.thisv(), Kind(slot / 16), &object)) return false;
  if (!argc) return WebKit::Fail(cx, "Geometry setter requires a value");
  double number;
  if (!JS::ToNumber(cx, args[0], &number)) return false;
  JSAutoRealm realm(cx, object);
  JS::SetReservedSlot(object, slot % 16, JS::NumberValue(number));
  args.rval().setUndefined();
  return true;
}
bool ScalarJSON(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  Kind kind = Kind(WebKit::Slot(args));
  JS::RootedObject object(cx);
  if (!Receiver(cx, args.thisv(), kind, &object)) return false;
  double values[4];
  for (unsigned i = 0; i < 4; ++i) values[i] = JS::GetReservedSlot(object, i).toNumber();
  return kind == RectReadOnly ? RectSnapshot(s, values, args.rval())
                             : PlainValues(s, kPointFields, values, 4, args.rval());
}
bool ScalarFrom(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  Kind kind = Kind(WebKit::Slot(args));
  bool point = kind >= PointReadOnly;
  double values[4] = {0, 0, 0, point ? 1.0 : 0.0};
  return ReadDictionary(s, args.get(0), point ? kPointFields : kRectFields, 4, values, true) &&
         NewValue(s, kind, values, nullptr, args.rval());
}
bool TransformPoint(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  JS::RootedObject object(cx);
  if (!Receiver(cx, args.thisv(), PointReadOnly, &object)) return false;
  return s.exception ? s.exception(s.exceptionContext, "NotSupportedError", "matrixTransform is not supported")
                     : WebKit::Fail(cx, "Geometry exception port is not installed");
}
}
