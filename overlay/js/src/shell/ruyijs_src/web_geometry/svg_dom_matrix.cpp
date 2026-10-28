#include "web_geometry/svg_dom_types.h"
#include "domain/geometry/svg_values.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include <cmath>

namespace ruyijs::web_geometry {
const JSClass kSvgMatrixClass = {"SVGMatrix", JSCLASS_HAS_RESERVED_SLOTS(6)};
namespace {
using M = domain::geometry::SvgMatrix;
constexpr double kRadPerDegree = 3.14159265358979323846 / 180.0;
const char* kFields[] = {"a", "b", "c", "d", "e", "f"};
bool Exception(State& s, const char* name, const std::string& message) {
  return s.exception ? s.exception(s.exceptionContext, name, message.c_str())
                     : WebKit::Fail(s.cx, message.c_str());
}
void Read(JSObject* object, double out[6]) {
  for (unsigned i = 0; i < 6; ++i) out[i] = JS::GetReservedSlot(object, i).toDouble();
}
bool ReadInit(JSContext* cx, JS::HandleValue value, double out[6]) {
  out[0] = out[3] = 1; out[1] = out[2] = out[4] = out[5] = 0;
  if (!value.isObject()) return true;
  JS::RootedObject object(cx, &value.toObject());
  for (unsigned i = 0; i < 6; ++i) {
    JS::RootedValue v(cx);
    if (!JS_GetProperty(cx, object, kFields[i], &v)) return false;
    if (v.isUndefined()) continue;
    double number;
    if (!JS::ToNumber(cx, v, &number)) return false;
    out[i] = number;
  }
  return true;
}
M Rotation(double angle) {
  return {std::cos(angle), std::sin(angle), -std::sin(angle), std::cos(angle), 0, 0};
}
bool Result(State& s, const M& m, JS::MutableHandleValue rval) {
  JS::RootedObject object(s.cx);
  const double components[6] = {m.a, m.b, m.c, m.d, m.e, m.f};
  if (!NewSvgMatrixObject(s, components, &object)) return false;
  rval.setObject(*object); return true;
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const unsigned field = WebKit::Slot(args);
  JS::RootedObject object(cx);
  if (!SvgBrandReceiver(cx, args.thisv(), &kSvgMatrixClass,
                        (std::string("get ") + kFields[field]).c_str(), &object)) return false;
  args.rval().setDouble(float(JS::GetReservedSlot(object, field).toDouble())); return true;
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const unsigned field = WebKit::Slot(args);
  JS::RootedObject object(cx);
  if (!SvgBrandReceiver(cx, args.thisv(), &kSvgMatrixClass,
                        (std::string("set ") + kFields[field]).c_str(), &object)) return false;
  double number;
  if (!JS::ToNumber(cx, args.get(0), &number)) return false;
  const float value = float(number);
  if (!std::isfinite(value)) return WebKit::Fail(cx,
      (std::string("SVGMatrix.") + kFields[field] +
       " setter: Value being assigned is not a finite floating-point value.").c_str());
  JS::SetReservedSlot(object, field, JS::DoubleValue(value)); args.rval().setUndefined(); return true;
}
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); State& s = Self(args);
  const unsigned op = WebKit::Slot(args);
  static const char* kNames[] = {"multiply", "inverse", "translate", "scale", "scaleNonUniform",
      "rotate", "rotateFromVector", "flipX", "flipY", "skewX", "skewY"};
  JS::RootedObject object(cx);
  if (!SvgBrandReceiver(cx, args.thisv(), &kSvgMatrixClass, kNames[op], &object)) return false;
  double c[6]; Read(object, c); M m{c[0], c[1], c[2], c[3], c[4], c[5]};
  if (op == 0) {
    double n[6];
    if (!ReadInit(cx, args.get(0), n)) return false;
    return Result(s, M{n[0], n[1], n[2], n[3], n[4], n[5]}.Then(m), args.rval());
  }
  if (op == 1) {
    const double det = m.a * m.d - m.b * m.c;
    if (!det) return Exception(s, "InvalidStateError", "SVGMatrix.inverse: Matrix is not invertible");
    return Result(s, {m.d / det, -m.b / det, -m.c / det, m.a / det,
                      (m.c * m.f - m.d * m.e) / det, (m.b * m.e - m.a * m.f) / det}, args.rval());
  }
  double x = 0, y = 0;
  if (op == 2 || op == 3 || op == 4 || op == 5 || op == 6 || op == 9 || op == 10) {
    if (!JS::ToNumber(cx, args.get(0), &x)) return false;
    if ((op == 2 || op == 4 || op == 6) && !JS::ToNumber(cx, args.get(1), &y)) return false;
    if (!std::isfinite(float(x)) || !std::isfinite(float(y))) return WebKit::Fail(cx, "Argument 1 is not a finite floating-point value.");
  }
  if (op == 2) return Result(s, m.Then({1, 0, 0, 1, x, y}), args.rval());
  if (op == 3) return Result(s, m.Then({x, 0, 0, x, 0, 0}), args.rval());
  if (op == 4) return Result(s, m.Then({x, 0, 0, y, 0, 0}), args.rval());
  if (op == 5) return Result(s, Rotation(x * kRadPerDegree).Then(m), args.rval());
  if (op == 6) {
    if (x == 0.0 || y == 0.0) return Exception(s, "InvalidAccessError", "SVGMatrix.rotateFromVector: Neither input parameter may be zero");
    return Result(s, Rotation(std::atan2(y, x)).Then(m), args.rval());
  }
  if (op == 7) return Result(s, {-m.a, -m.b, m.c, m.d, m.e, m.f}, args.rval());
  if (op == 8) return Result(s, {m.a, m.b, -m.c, -m.d, m.e, m.f}, args.rval());
  const double ta = std::tan(x * kRadPerDegree);
  if (!std::isfinite(ta)) return Exception(s, "InvalidAccessError", "Invalid angle");
  if (op == 9) return Result(s, {m.a, m.b, m.c + m.a * ta, m.d + m.b * ta, m.e, m.f}, args.rval());
  return Result(s, {m.a + m.c * ta, m.b + m.d * ta, m.c, m.d, m.e, m.f}, args.rval());
}
}
bool NewSvgMatrixObject(State& s, const double components[6], JS::MutableHandleObject out) {
  out.set(JS_NewObjectWithGivenProto(s.cx, &kSvgMatrixClass, s.svgMatrix.prototype));
  if (!out) return false;
  for (unsigned i = 0; i < 6; ++i) JS::SetReservedSlot(out, i, JS::DoubleValue(components[i]));
  return true;
}
bool InstallSvgMatrix(State& s) {
  JS::RootedObject plain(s.cx, JS::GetRealmObjectPrototype(s.cx));
  if (!s.kit.Define("SVGMatrix", &kSvgMatrixClass, plain, WebKit::Reject, &s.svgMatrix) ||
      !JS_DeleteProperty(s.cx, s.svgMatrix.prototype, "constructor")) return false;
  const char* methods[] = {"multiply", "inverse", "translate", "scale", "scaleNonUniform", "rotate",
      "rotateFromVector", "flipX", "flipY", "skewX", "skewY"};
  const unsigned lengths[] = {0, 0, 2, 1, 2, 1, 2, 0, 0, 1, 1};
  for (unsigned i = 0; i < 11; ++i)
    if (!s.kit.Method(s.svgMatrix, "SVGMatrix", methods[i], Call, lengths[i], i)) return false;
  for (unsigned i = 0; i < 6; ++i)
    if (!s.kit.Accessor(s.svgMatrix, "SVGMatrix", kFields[i], Get, Set, i)) return false;
  return JS_DefineProperty(s.cx, s.svgMatrix.prototype, "constructor", s.svgMatrix.constructor, 0);
}
}
