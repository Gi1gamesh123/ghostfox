#include "web_geometry/svg_dom_types.h"
#include "js/Realm.h"
#include "js/String.h"
#include "js/Wrapper.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace ruyijs::web_geometry {
const JSClass kSvgAngleClass = {"SVGAngle", JSCLASS_HAS_RESERVED_SLOTS(2)};
namespace {
constexpr const char* kUnits[] = {"", "", "deg", "rad", "grad"};
constexpr double kRadPerDegree = 3.14159265358979323846 / 180.0;
const char* kFields[] = {"unitType", "value", "valueInSpecifiedUnits", "valueAsString"};
bool Exception(State& s, const char* name, const std::string& message) {
  return s.exception ? s.exception(s.exceptionContext, name, message.c_str())
                     : WebKit::Fail(s.cx, message.c_str());
}
double ToDegrees(double specified, unsigned unit) {
  if (unit == 3) return specified / kRadPerDegree;
  if (unit == 4) return specified * 0.9;
  return specified;
}
double FromDegrees(double degrees, unsigned unit) {
  if (unit == 3) return degrees * kRadPerDegree;
  if (unit == 4) return degrees / 0.9;
  return degrees;
}
std::string Serialize(double degrees, unsigned unit) {
  char buffer[64];
  std::snprintf(buffer, sizeof buffer, "%g", float(FromDegrees(degrees, unit)));
  return std::string(buffer) + kUnits[unit <= 4 ? unit : 0];
}
bool Parse(const std::u16string& text, double* degrees, unsigned* unit) {
  const char16_t* p = text.data(), *end = p + text.size();
  while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
  const char16_t* numberStart = p;
  if (p < end && (*p == '+' || *p == '-')) ++p;
  bool digit = false;
  while (p < end && *p >= '0' && *p <= '9') { ++p; digit = true; }
  if (p < end && *p == '.') { ++p; while (p < end && *p >= '0' && *p <= '9') { ++p; digit = true; } }
  if (!digit) return false;
  if (p + 1 < end && (*p == 'e' || *p == 'E') &&
      (p[1] >= '0' && p[1] <= '9' || p[1] == '+' || p[1] == '-')) {
    ++p; if (p < end && (*p == '+' || *p == '-')) ++p;
    const char16_t* e = p; while (p < end && *p >= '0' && *p <= '9') ++p;
    if (p == e) return false;
  }
  std::string number;
  for (const char16_t* q = numberStart; q < p; ++q) number += char(*q);
  std::string suffix;
  for (const char16_t* q = p; q < end; ++q) {
    char16_t c = *q;
    if (c >= 'A' && c <= 'Z') c += 32;
    suffix += char(c);
  }
  unsigned u = 0;
  if (suffix.empty()) u = 1;
  else for (unsigned i = 2; i <= 4; ++i) if (suffix == kUnits[i]) { u = i; break; }
  if (!u) return false;
  char* stop = nullptr;
  const double parsed = std::strtod(number.c_str(), &stop);
  if (!stop || *stop || !std::isfinite(parsed)) return false;
  *unit = u;
  *degrees = ToDegrees(parsed, u);
  return true;
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); State& s = Self(args);
  const unsigned field = WebKit::Slot(args);
  JS::RootedObject object(cx);
  if (!SvgBrandReceiver(cx, args.thisv(), &kSvgAngleClass,
                        (std::string("get ") + kFields[field]).c_str(), &object)) return false;
  const double degrees = JS::GetReservedSlot(object, 0).toDouble();
  const unsigned unit = JS::GetReservedSlot(object, 1).toInt32();
  if (field == 0) args.rval().setInt32(unit <= 4 ? int32_t(unit) : 0);
  else if (field == 1) args.rval().setDouble(float(degrees));
  else if (field == 2) args.rval().setDouble(float(FromDegrees(degrees, unit)));
  else {
    const auto text = Serialize(degrees, unit);
    JS::RootedString string(cx, JS_NewStringCopyN(cx, text.data(), text.size()));
    if (!string) return false;
    args.rval().setString(string);
  }
  return true;
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); State& s = Self(args);
  const unsigned field = WebKit::Slot(args);
  JS::RootedObject object(cx);
  if (!SvgBrandReceiver(cx, args.thisv(), &kSvgAngleClass,
                        (std::string("set ") + kFields[field]).c_str(), &object)) return false;
  const unsigned unit = JS::GetReservedSlot(object, 1).toInt32();
  double degrees = JS::GetReservedSlot(object, 0).toDouble();
  if (field == 2) {
    double number;
    if (!JS::ToNumber(cx, args.get(0), &number)) return false;
    degrees = ToDegrees(number, unit);
  } else if (field == 1) {
    double number;
    if (!JS::ToNumber(cx, args.get(0), &number)) return false;
    degrees = number;
  } else {
    JS::RootedString string(cx, JS::ToString(cx, args.get(0)));
    if (!string) return false;
    std::u16string text;
    text.resize(JS_GetStringLength(string));
    if (!text.empty() && !JS::CopyStringChars(cx, text.data(), string, text.size())) return false;
    unsigned parsedUnit = 0;
    if (!Parse(text, &degrees, &parsedUnit))
      return Exception(s, "SyntaxError", "An invalid or illegal string was specified");
    JS::SetReservedSlot(object, 1, JS::Int32Value(parsedUnit));
  }
  if (!std::isfinite(float(degrees))) return WebKit::Fail(cx,
      (std::string("SVGAngle.") + kFields[field] +
       " setter: Value being assigned is not a finite floating-point value.").c_str());
  JS::SetReservedSlot(object, 0, JS::DoubleValue(float(degrees)));
  args.rval().setUndefined(); return true;
}
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); State& s = Self(args);
  const bool convert = WebKit::Slot(args) != 0;
  const char* name = convert ? "convertToSpecifiedUnits" : "newValueSpecifiedUnits";
  const auto member = std::string("SVGAngle.") + name;
  JS::RootedObject object(cx);
  if (!SvgBrandReceiver(cx, args.thisv(), &kSvgAngleClass, name, &object)) return false;
  const unsigned required = convert ? 1 : 2;
  if (argc < required) return WebKit::Fail(cx, (member + ": At least " + std::to_string(required) +
      " argument" + (required == 1 ? "" : "s") + " required, but only " + std::to_string(argc) +
      " passed").c_str());
  uint32_t rawUnit;
  if (!JS::ToUint32(cx, args[0], &rawUnit)) return false;
  double number = 0;
  if (!convert) {
    if (!JS::ToNumber(cx, args[1], &number)) return false;
    if (!std::isfinite(float(number))) return WebKit::Fail(cx,
        (member + ": Argument 2 is not a finite floating-point value.").c_str());
  }
  const unsigned unit = rawUnit & 0xffff;
  if (!unit || unit > 4) return Exception(s, "NotSupportedError", member + ": Unknown unit type");
  double degrees = JS::GetReservedSlot(object, 0).toDouble();
  if (convert) degrees = JS::GetReservedSlot(object, 0).toDouble();
  else degrees = ToDegrees(number, unit);
  JS::SetReservedSlot(object, 0, JS::DoubleValue(float(degrees)));
  JS::SetReservedSlot(object, 1, JS::Int32Value(unit));
  args.rval().setUndefined(); return true;
}
}
bool NewSvgAngleObject(State& s, double degrees, unsigned unit, JS::MutableHandleObject out) {
  out.set(JS_NewObjectWithGivenProto(s.cx, &kSvgAngleClass, s.svgAngle.prototype));
  if (!out) return false;
  JS::SetReservedSlot(out, 0, JS::DoubleValue(float(degrees)));
  JS::SetReservedSlot(out, 1, JS::Int32Value(unit));
  return true;
}
bool InstallSvgAngle(State& s) {
  JS::RootedObject plain(s.cx, JS::GetRealmObjectPrototype(s.cx));
  if (!s.kit.Define("SVGAngle", &kSvgAngleClass, plain, WebKit::Reject, &s.svgAngle) ||
      !JS_DeleteProperty(s.cx, s.svgAngle.prototype, "constructor") ||
      !s.kit.Method(s.svgAngle, "SVGAngle", "newValueSpecifiedUnits", Call, 2, 0) ||
      !s.kit.Method(s.svgAngle, "SVGAngle", "convertToSpecifiedUnits", Call, 1, 1)) return false;
  for (unsigned i = 0; i < 4; ++i)
    if (!s.kit.Accessor(s.svgAngle, "SVGAngle", kFields[i], Get, i ? Set : nullptr, i)) return false;
  const char* names[] = {"UNKNOWN", "UNSPECIFIED", "DEG", "RAD", "GRAD"};
  for (unsigned i = 0; i < 5; ++i) {
    const auto name = std::string("SVG_ANGLETYPE_") + names[i];
    const unsigned flags = JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;
    if (!JS_DefineProperty(s.cx, s.svgAngle.prototype, name.c_str(), i, flags) ||
        !JS_DefineProperty(s.cx, s.svgAngle.constructor, name.c_str(), i, flags)) return false;
  }
  return JS_DefineProperty(s.cx, s.svgAngle.prototype, "constructor", s.svgAngle.constructor, 0);
}
}
