#include "web_geometry/svg_dom_types.h"
#include "domain/geometry/svg_values.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include <cmath>

namespace ruyijs::web_geometry {
const JSClass kSvgTransformClass = {"SVGTransform", JSCLASS_HAS_RESERVED_SLOTS(8)};
namespace {
constexpr double kRadPerDegree = 3.14159265358979323846 / 180.0;
const char* kMethods[] = {"setMatrix", "setTranslate", "setScale", "setRotate", "setSkewX", "setSkewY"};
void ReadComponents(JSObject* object, double out[6]) {
  for (unsigned i = 0; i < 6; ++i) out[i] = JS::GetReservedSlot(object, i + 1).toDouble();
}
bool ReadInit(JSContext* cx, JS::HandleValue value, double out[6]) {
  out[0] = out[3] = 1; out[1] = out[2] = out[4] = out[5] = 0;
  if (!value.isObject()) return true;
  JS::RootedObject object(cx, &value.toObject());
  static const char* names[6] = {"a", "b", "c", "d", "e", "f"};
  for (unsigned i = 0; i < 6; ++i) {
    JS::RootedValue v(cx);
    if (!JS_GetProperty(cx, object, names[i], &v)) return false;
    if (v.isUndefined()) continue;
    double number;
    if (!JS::ToNumber(cx, v, &number)) return false;
    out[i] = number;
  }
  return true;
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); State& s = Self(args);
  const unsigned field = WebKit::Slot(args);
  static const char* names[3] = {"type", "matrix", "angle"};
  JS::RootedObject object(cx);
  if (!SvgBrandReceiver(cx, args.thisv(), &kSvgTransformClass,
                        (std::string("get ") + names[field]).c_str(), &object)) return false;
  if (field == 0) args.rval().setInt32(JS::GetReservedSlot(object, 0).toInt32());
  else if (field == 2) args.rval().setDouble(float(JS::GetReservedSlot(object, 7).toDouble()));
  else {
    double components[6]; ReadComponents(object, components);
    JS::RootedObject matrix(cx);
    if (!NewSvgMatrixObject(s, components, &matrix)) return false;
    args.rval().setObject(*matrix);
  }
  return true;
}
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); State& s = Self(args);
  const unsigned op = WebKit::Slot(args);
  JS::RootedObject object(cx);
  if (!SvgBrandReceiver(cx, args.thisv(), &kSvgTransformClass, kMethods[op], &object)) return false;
  if (op == 0) {
    double m[6];
    if (!ReadInit(cx, args.get(0), m)) return false;
    JS::SetReservedSlot(object, 0, JS::Int32Value(1));
    for (unsigned i = 0; i < 6; ++i) JS::SetReservedSlot(object, i + 1, JS::DoubleValue(m[i]));
    JS::SetReservedSlot(object, 7, JS::DoubleValue(0));
    args.rval().setUndefined(); return true;
  }
  const unsigned needed = op == 1 || op == 2 ? 2 : op == 3 ? 3 : 1;
  if (argc < needed) return WebKit::Fail(cx, (std::string("SVGTransform.") + kMethods[op] +
      ": Not enough arguments").c_str());
  double values[3] = {};
  for (unsigned i = 0; i < needed; ++i)
    if (!JS::ToNumber(cx, args.get(i), &values[i])) return false;
  const float angle = float(values[0]);
  double components[6] = {1, 0, 0, 1, 0, 0};
  unsigned type = 0;
  if (op == 1) { type = 2; components[4] = values[0]; components[5] = values[1]; }
  else if (op == 2) { type = 3; components[0] = values[0]; components[3] = values[1]; }
  else if (op == 3) {
    type = 4;
    const double rad = double(angle) * kRadPerDegree;
    const double co = std::cos(rad), si = std::sin(rad);
    components[0] = co; components[1] = si; components[2] = -si; components[3] = co;
    components[4] = values[1] - values[1] * co + values[2] * si;
    components[5] = values[2] - values[1] * si - values[2] * co;
  } else if (op == 4) { type = 5; components[2] = std::tan(double(angle) * kRadPerDegree); }
  else { type = 6; components[1] = std::tan(double(angle) * kRadPerDegree); }
  JS::SetReservedSlot(object, 0, JS::Int32Value(type));
  for (unsigned i = 0; i < 6; ++i) JS::SetReservedSlot(object, i + 1, JS::DoubleValue(components[i]));
  JS::SetReservedSlot(object, 7, JS::DoubleValue(op >= 3 ? angle : 0));
  args.rval().setUndefined(); return true;
}
}
bool NewSvgTransformObject(State& s, unsigned type, const double components[6], double angle,
                           JS::MutableHandleObject out) {
  out.set(JS_NewObjectWithGivenProto(s.cx, &kSvgTransformClass, s.svgTransform.prototype));
  if (!out) return false;
  JS::SetReservedSlot(out, 0, JS::Int32Value(type));
  for (unsigned i = 0; i < 6; ++i) JS::SetReservedSlot(out, i + 1, JS::DoubleValue(components[i]));
  JS::SetReservedSlot(out, 7, JS::DoubleValue(angle));
  return true;
}
bool InstallSvgTransform(State& s) {
  JS::RootedObject plain(s.cx, JS::GetRealmObjectPrototype(s.cx));
  if (!s.kit.Define("SVGTransform", &kSvgTransformClass, plain, WebKit::Reject, &s.svgTransform) ||
      !JS_DeleteProperty(s.cx, s.svgTransform.prototype, "constructor")) return false;
  const unsigned lengths[] = {0, 2, 2, 3, 1, 1};
  for (unsigned i = 0; i < 6; ++i)
    if (!s.kit.Method(s.svgTransform, "SVGTransform", kMethods[i], Call, lengths[i], i)) return false;
  const char* fields[] = {"type", "matrix", "angle"};
  for (unsigned i = 0; i < 3; ++i)
    if (!s.kit.Accessor(s.svgTransform, "SVGTransform", fields[i], Get, nullptr, i)) return false;
  const char* names[] = {"UNKNOWN", "MATRIX", "TRANSLATE", "SCALE", "ROTATE", "SKEWX", "SKEWY"};
  for (unsigned i = 0; i < 7; ++i) {
    const auto name = std::string("SVG_TRANSFORM_") + names[i];
    const unsigned flags = JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;
    if (!JS_DefineProperty(s.cx, s.svgTransform.prototype, name.c_str(), i, flags) ||
        !JS_DefineProperty(s.cx, s.svgTransform.constructor, name.c_str(), i, flags)) return false;
  }
  return JS_DefineProperty(s.cx, s.svgTransform.prototype, "constructor", s.svgTransform.constructor, 0);
}
}
