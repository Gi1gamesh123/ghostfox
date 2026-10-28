#include "web_geometry/svg_length.h"
#include "js/Exception.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::web_geometry {
const JSClass kSvgLengthClass = {"SVGLength", JSCLASS_HAS_RESERVED_SLOTS(5)};
bool SvgLengthReceiver(JSContext* cx, JS::HandleValue value, const std::string& operation,
                       JS::MutableHandleObject out) {
  out.set(value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  return out && JS::GetClass(out) == &kSvgLengthClass ? true :
      WebKit::Fail(cx, ("'" + operation + "' called on an object that does not implement interface SVGLength.").c_str());
}
bool SvgLengthRead(State& s, JS::HandleObject object, SvgLengthValue* out) {
  if (!JS::GetReservedSlot(object, LengthMode).toInt32()) {
    *out = {float(JS::GetReservedSlot(object, LengthNumber).toNumber()),
            uint16_t(JS::GetReservedSlot(object, LengthUnit).toInt32()), true};
    return true;
  }
  JS::RootedValue node(s.cx, JS::GetReservedSlot(object, LengthNode));
  void* factory = nullptr;
  if (!JS_WrapValue(s.cx, &node) || !s.svgPorts.lengthRead ||
      !s.svgPorts.lengthRead(s.svgPorts.context, node,
          JS::GetReservedSlot(object, LengthField).toInt32(), out, &factory)) return false;
  return true;
}
bool SvgLengthWrite(State& s, JS::HandleObject object, const SvgLengthValue& value) {
  if (!JS::GetReservedSlot(object, LengthMode).toInt32()) {
    JSAutoRealm realm(s.cx, object);
    JS::SetReservedSlot(object, LengthNumber, JS::DoubleValue(value.number));
    JS::SetReservedSlot(object, LengthUnit, JS::Int32Value(value.unit));
    return true;
  }
  JS::RootedValue node(s.cx, JS::GetReservedSlot(object, LengthNode));
  return JS_WrapValue(s.cx, &node) && s.svgPorts.lengthWrite &&
      s.svgPorts.lengthWrite(s.svgPorts.context, node,
          JS::GetReservedSlot(object, LengthField).toInt32(), value);
}
bool SvgLengthException(State& s, const char* name, const std::string& message) {
  return s.exception ? s.exception(s.exceptionContext, name, message.c_str()) : WebKit::Fail(s.cx, message.c_str());
}
bool SvgLengthWritable(State& s, JS::HandleObject object, const std::string& operation) {
  return JS::GetReservedSlot(object, LengthMode).toInt32() != 2 ||
      SvgLengthException(s, "NoModificationAllowedError", "SVGLength." + operation + ": Animated values cannot be set");
}
bool SvgLengthFloat(JSContext* cx, JS::HandleValue value, const std::string& operation, float* out) {
  double converted;
  if (!JS::ToNumber(cx, value, &converted)) return false;
  *out = float(converted);
  return std::isfinite(*out) || WebKit::Fail(cx,
      ("SVGLength." + operation + ": Value being assigned is not a finite floating-point value.").c_str());
}
bool NewSvgLengthObject(State& s, JS::HandleObject node, unsigned field, unsigned mode,
                         JS::MutableHandleObject result) {
  result.set(JS_NewObjectWithGivenProto(s.cx, &kSvgLengthClass, s.svgLength.prototype));
  if (!result) return false;
  JS::SetReservedSlot(result, LengthNumber, JS::DoubleValue(0));
  JS::SetReservedSlot(result, LengthUnit, JS::Int32Value(1));
  JS::SetReservedSlot(result, LengthNode, JS::ObjectOrNullValue(node));
  JS::SetReservedSlot(result, LengthField, JS::Int32Value(field));
  JS::SetReservedSlot(result, LengthMode, JS::Int32Value(mode));
  return true;
}
bool Realm::NewSvgLength(JS::HandleObject, JS::MutableHandleValue result) {
  State& s = *state_;
  {
    JSAutoRealm realm(s.cx, s.kit.page);
    JS::RootedObject object(s.cx), empty(s.cx);
    if (!NewSvgLengthObject(s, empty, 0, 0, &object)) return false;
    result.setObject(*object);
  }
  return JS_WrapValue(s.cx, result);
}
bool InstallSvgLength(State& s) {
  JS::RootedObject plain(s.cx, JS::GetRealmObjectPrototype(s.cx));
  if (!s.kit.Define("SVGLength", &kSvgLengthClass, plain, WebKit::Reject, &s.svgLength) ||
      !JS_DeleteProperty(s.cx, s.svgLength.prototype, "constructor") ||
      !s.kit.Method(s.svgLength, "SVGLength", "newValueSpecifiedUnits", SvgLengthCall, 2, 0) ||
      !s.kit.Method(s.svgLength, "SVGLength", "convertToSpecifiedUnits", SvgLengthCall, 1, 1)) return false;
  const char* fields[] = {"unitType", "value", "valueInSpecifiedUnits", "valueAsString"};
  for (unsigned i = 0; i < 4; ++i)
    if (!s.kit.Accessor(s.svgLength, "SVGLength", fields[i], SvgLengthGet, i ? SvgLengthSet : nullptr, i)) return false;
  const char* names[] = {"UNKNOWN", "NUMBER", "PERCENTAGE", "EMS", "EXS", "PX", "CM", "MM", "IN", "PT", "PC"};
  for (unsigned i = 0; i < 11; ++i) {
    const auto name = std::string("SVG_LENGTHTYPE_") + names[i];
    const unsigned flags = JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;
    if (!JS_DefineProperty(s.cx, s.svgLength.prototype, name.c_str(), i, flags) ||
        !JS_DefineProperty(s.cx, s.svgLength.constructor, name.c_str(), i, flags)) return false;
  }
  return JS_DefineProperty(s.cx, s.svgLength.prototype, "constructor", s.svgLength.constructor, 0) &&
      InstallSvgAnimatedLength(s);
}
}
