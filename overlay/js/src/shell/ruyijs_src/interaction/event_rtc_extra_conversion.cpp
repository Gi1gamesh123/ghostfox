#include "interaction/internal.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include <cmath>

namespace ruyijs::interaction {
bool RtcEventUnsignedShort(JSContext* cx, JS::HandleValue value, uint16_t* out) {
  *out = 0;
  double number = 0;
  if (!JS::ToNumber(cx, value, &number)) {
    if (!JS_IsExceptionPending(cx) || JS_IsThrowingOutOfMemory(cx)) return false;
    JS_ClearPendingException(cx);
    return true;
  }
  if (!std::isfinite(number)) return true;
  // D121: public ruyijs's negative object integers vary with V8's internal number
  // representation. Keep a deterministic fallback; direct numeric inputs wrap.
  if (number < 0 && (value.isString() || (value.isObject() && std::trunc(number) == number)))
    return true;
  *out = static_cast<uint16_t>(JS::ToUint32(number));
  return true;
}
}  // namespace ruyijs::interaction
