#include <cmath>
#include "web_geometry/svg_length.h"
#include "js/String.h"
#include "js/Exception.h"

namespace ruyijs::web_geometry {
namespace {
const char* kFields[] = {"unitType", "value", "valueInSpecifiedUnits", "valueAsString"};
// Firefox throws an XPCOM NS_ERROR_FAILURE Exception here; ruyijs has no XPCOM
// Exception owner, so this is an Error carrying the same name and empty message
// (recorded deviation, docs/owners/WEB_GEOMETRY.md).
bool DetachedRelativeError(State& s, const char* label = "NS_ERROR_FAILURE") {
  WebKit::FailKind(s.cx, "", JSEXN_ERR);
  JS::RootedValue error(s.cx);
  if (!JS_GetPendingException(s.cx, &error) || !error.isObject()) return false;
  JS_ClearPendingException(s.cx);
  JS::RootedObject object(s.cx, &error.toObject());
  JS::RootedString name(s.cx, JS_NewStringCopyZ(s.cx, label));
  if (!name || !JS_DefineProperty(s.cx, object, "name", name, 0)) return false;
  JS_SetPendingException(s.cx, error);
  return false;
}
}
bool SvgLengthGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  const unsigned field = WebKit::Slot(args);
  JS::RootedObject object(cx);
  SvgLengthValue value;
  if (!SvgLengthReceiver(cx, args.thisv(), std::string("get ") + kFields[field], &object) ||
      !SvgLengthRead(s, object, &value)) return false;
  if (field == 0) args.rval().setInt32(value.unit <= 10 ? value.unit : 0);
  else if (field == 2) args.rval().setDouble(value.number);
  else if (field == 3) {
    const auto text = domain::geometry::SerializeSvgLength(value);
    JS::RootedString string(cx, JS_NewStringCopyN(cx, text.data(), text.size()));
    if (!string) return false;
    args.rval().setString(string);
  } else {
    if (!JS::GetReservedSlot(object, LengthMode).toInt32() && !domain::geometry::SvgLengthAbsolute(value.unit))
      return DetachedRelativeError(s);
    float factor;
    if (!SvgLengthFactor(s, object, value.unit, &factor)) return false;
    args.rval().setDouble(float(value.number * factor));
  }
  return true;
}
bool SvgLengthSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  const unsigned field = WebKit::Slot(args);
  const std::string operation = std::string(kFields[field]) + " setter";
  JS::RootedObject object(cx);
  if (!SvgLengthReceiver(cx, args.thisv(), std::string("set ") + kFields[field], &object)) return false;
  SvgLengthValue value;
  std::u16string text;
  float number = 0;
  if (field == 3) {
    JS::RootedString string(cx, JS::ToString(cx, args.get(0)));
    if (!string) return false;
    text.resize(JS_GetStringLength(string));
    if (!text.empty() && !JS::CopyStringChars(cx, text.data(), string, text.size())) return false;
  } else if (!SvgLengthFloat(cx, args.get(0), operation, &number)) return false;
  if (!SvgLengthWritable(s, object, operation) || !SvgLengthRead(s, object, &value)) return false;
  const bool live = JS::GetReservedSlot(object, LengthMode).toInt32() != 0;
  if (field == 3) {
    const unsigned attribute = JS::GetReservedSlot(object, LengthField).toInt32();
    if (!domain::geometry::ParseSvgLength(text, &value, live) ||
        (live && value.number < 0 && domain::document_model::SvgLengthNonNegative(attribute))) {
      if (live) return SvgLengthException(s, "SyntaxError", "An invalid or illegal string was specified");
      std::string message = "SVGLength.valueAsString setter: Cannot parse ";
      JS::RootedString string(cx, JS_NewUCStringCopyN(cx, text.data(), text.size()));
      if (!string) return false;
      JS::UniqueChars utf8 = JS_EncodeStringToUTF8(cx, string);
      if (!utf8) return false;
      message.append(utf8.get());
      return SvgLengthException(s, "SyntaxError", message);
    }
  } else if (field == 2) value.number = number;
  else {
    if (!live && !domain::geometry::SvgLengthAbsolute(value.unit)) return DetachedRelativeError(s);
    if (!live) value.number = number * domain::geometry::SvgLengthAbsoluteScale(value.unit, 5);
    else {
      float factor;
      if (!SvgLengthFactor(s, object, value.unit, &factor)) return false;
      value.number = number / factor;
      if (!std::isfinite(value.number))
        return DetachedRelativeError(s, "NS_ERROR_ILLEGAL_VALUE");
    }
  }
  value.specified = true;
  if (!SvgLengthWrite(s, object, value)) return false;
  args.rval().setUndefined(); return true;
}
bool SvgLengthCall(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  const bool convert = WebKit::Slot(args) != 0;
  const std::string method = convert ? "convertToSpecifiedUnits" : "newValueSpecifiedUnits";
  JS::RootedObject object(cx);
  if (!SvgLengthReceiver(cx, args.thisv(), method, &object)) return false;
  const unsigned required = convert ? 1 : 2;
  if (argc < required) return WebKit::Fail(cx, ("SVGLength." + method + ": At least " +
      std::to_string(required) + " argument" + (required == 1 ? "" : "s") + " required, but only " +
      std::to_string(argc) + " passed").c_str());
  uint32_t rawUnit;
  float number = 0;
  if (!JS::ToUint32(cx, args[0], &rawUnit)) return false;
  if (!convert) {
    double converted;
    if (!JS::ToNumber(cx, args[1], &converted)) return false;
    number = float(converted);
    if (!std::isfinite(number)) return WebKit::Fail(cx,
        "SVGLength.newValueSpecifiedUnits: Argument 2 is not a finite floating-point value.");
  }
  if (!SvgLengthWritable(s, object, method)) return false;
  const unsigned unit = rawUnit & 0xffff;
  if (!unit || unit > 10) return SvgLengthException(s, "NotSupportedError", "SVGLength." + method + ": Unknown unit type");
  SvgLengthValue value;
  if (!SvgLengthRead(s, object, &value)) return false;
  if (convert) {
    if (value.unit == unit && value.specified) { args.rval().setUndefined(); return true; }
    if (domain::geometry::SvgLengthAbsolute(unit) && domain::geometry::SvgLengthAbsolute(value.unit))
      number = value.number * domain::geometry::SvgLengthAbsoluteScale(unit, value.unit);
    else {
      float current, target;
      if (!SvgLengthFactor(s, object, value.unit, &current) || !SvgLengthFactor(s, object, unit, &target)) return false;
      number = float(value.number * current) / target;
    }
    if (!std::isfinite(number)) return WebKit::Fail(cx, "SVGLength.convertToSpecifiedUnits: value is not a finite floating-point value.");
  }
  if (!SvgLengthWrite(s, object, {number, uint16_t(unit), true})) return false;
  args.rval().setUndefined(); return true;
}
}
