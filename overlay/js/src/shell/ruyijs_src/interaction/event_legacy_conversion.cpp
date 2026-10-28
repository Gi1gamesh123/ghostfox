#include "interaction/event_legacy_conversion.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/Wrapper.h"
#include <cmath>
#include <limits>

namespace ruyijs::interaction {
bool LegacyConversion::Capture() {
  if (!JS_IsExceptionPending(cx_) || JS_IsThrowingOutOfMemory(cx_)) return false;
  if (!JS_GetPendingException(cx_, &exception_)) return false;
  JS_ClearPendingException(cx_);
  failed_ = true;
  return true;
}
bool LegacyConversion::String(JS::HandleValue input, JS::MutableHandleValue out,
                               bool empty_on_failure) {
  JS::RootedString text(cx_, JS::ToString(cx_, input));
  if (!text) {
    if (!Capture()) return false;
    text = input.isObject() && !empty_on_failure
        ? JS_NewStringCopyZ(cx_, "[object Object]") : JS_GetEmptyString(cx_);
    if (!text) return false;
  }
  if (JS_GetStringLength(text) > 1024 * 1024)
    return Error(cx_, "RangeError", "Event initializer text exceeds limit");
  out.setString(text);
  return true;
}
bool LegacyConversion::Int(JS::HandleValue input, int32_t* out) {
  *out = 0;
  if (JS::ToInt32(cx_, input, out)) return true;
  *out = 0;
  return Capture();
}
bool LegacyConversion::Float(JS::HandleValue input, float* out) {
  double number = 0.0;
  if (!JS::ToNumber(cx_, input, &number)) {
    *out = 0.0f;
    return Capture();
  }
  if (!std::isfinite(number) ||
      number > std::numeric_limits<float>::max() ||
      number < -std::numeric_limits<float>::max()) {
    *out = 0.0f;
    Error(cx_, "TypeError", "MouseEvent pressure must be a finite float");
    return Capture();
  }
  *out = static_cast<float>(number);
  return true;
}
bool LegacyConversion::Uint16(JS::HandleValue input, uint16_t* out) {
  *out = 0;
  if (JS::ToUint16(cx_, input, out)) return true;
  *out = 0;
  return Capture();
}
bool LegacyConversion::Type(JS::HandleValue input) {
  JS::RootedValue converted(cx_);
  return String(input, &converted) && Text(cx_, converted, &type_, &key_);
}
void LegacyConversion::Commit(Event* event, const JS::CallArgs& args) {
  event->type.set(type_);
  event->value.initialize_legacy(domain::EventType(std::move(key_)),
      JS::ToBoolean(args.get(1)), JS::ToBoolean(args.get(2)));
}
bool LegacyConversion::Finish() {
  if (!failed_) return true;
  JS_SetPendingException(cx_, exception_);
  return false;
}
JS::Value LegacyValue(const JS::CallArgs& args, unsigned index) {
  return args.hasDefined(index) ? args[index].get() : JS::NullValue();
}
JS::Value LegacyTarget(JSContext* cx, JS::HandleValue value) {
  if (value.isObject()) {
    JSObject* object = js::CheckedUnwrapStatic(&value.toObject());
    if (object && Owner(cx, object, false)) return value.get();
  }
  return JS::NullValue();
}
uint32_t LegacyModifiers(const JS::CallArgs& args, unsigned first) {
  return (JS::ToBoolean(args.get(first)) ? domain::kControlMask : 0) |
         (JS::ToBoolean(args.get(first + 1)) ? domain::kAltMask : 0) |
         (JS::ToBoolean(args.get(first + 2)) ? domain::kShiftMask : 0) |
         (JS::ToBoolean(args.get(first + 3)) ? domain::kMetaMask : 0);
}
}
