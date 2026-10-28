#include "web_geometry/matrix.h"
#include "js/Realm.h"

namespace ruyijs::web_geometry {
bool MatrixGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  unsigned slot = WebKit::Slot(args), field = slot % 32;
  JS::RootedObject object(cx);
  if (!MatrixReceiver(cx, args.thisv(), slot < 32, &object)) return false;
  if (field < 22) args.rval().set(JS::GetReservedSlot(object, MatrixIndex(field)));
  else if (field == 22) args.rval().set(JS::GetReservedSlot(object, 16));
  else {
    double values[16]; ReadMatrix(object, values);
    args.rval().setBoolean(domain::geometry::MatrixIsIdentity(values));
  }
  return true;
}
bool MatrixSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  unsigned index = MatrixIndex(WebKit::Slot(args) % 32);
  JS::RootedObject object(cx);
  if (!MatrixReceiver(cx, args.thisv(), false, &object)) return false;
  if (!argc) return WebKit::Fail(cx, "DOMMatrix setter requires a value");
  double number;
  if (!JS::ToNumber(cx, args[0], &number)) return false;
  JSAutoRealm realm(cx, object);
  JS::SetReservedSlot(object, index, JS::NumberValue(number));
  if (!domain::geometry::MatrixPreserves2D(index, number))
    JS::SetReservedSlot(object, 16, JS::BooleanValue(false));
  args.rval().setUndefined();
  return true;
}
bool MatrixMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  auto op = MatrixOperation(WebKit::Slot(args));
  bool readonly = op != SameMatrix && op != TranslateMatrix && op != SetMatrixString;
  JS::RootedObject object(cx);
  if (!MatrixReceiver(cx, args.thisv(), readonly, &object)) return false;
  if (op == NewIdentity) {
    double values[16]; domain::geometry::MatrixIdentity(values);
    return NewMatrix(s, true, values, true, nullptr, args.rval());
  }
  if (op == TranslateMatrix) {
    double delta[3] = {};
    for (unsigned i = 0; i < 3 && i < argc; ++i)
      if (!JS::ToNumber(cx, args[i], &delta[i])) return false;
    JSAutoRealm realm(cx, object);
    double values[16]; ReadMatrix(object, values);
    domain::geometry::MatrixTranslate(values, delta);
    for (unsigned i = 12; i < 15; ++i) JS::SetReservedSlot(object, i, JS::NumberValue(values[i]));
    if (delta[2] != 0) JS::SetReservedSlot(object, 16, JS::BooleanValue(false));
  } else if (op == SetMatrixString) {
    if (!argc) return WebKit::Fail(cx, "setMatrixValue requires a string");
    JS::RootedString text(cx, JS::ToString(cx, args[0]));
    if (!text) return false;
  } else if (op == MatrixPoint) {
    args.rval().setUndefined(); return true;
  } else if (op != SameMatrix) {
    return MatrixExport(s, object, op, args.rval());
  }
  args.rval().set(args.thisv());
  return true;
}
}
