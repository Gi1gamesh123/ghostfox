#include "document_model/html_control.h"
#include <cmath>
#include <cstdio>

namespace ruyijs::document_model {
bool ControlString(JSContext* cx, JS::HandleValue value, DomString* out) {
  if (!value.isNumber()) return String(cx, value, out);
  const double number = value.toNumber();
  if (std::isfinite(number) && std::trunc(number) == number && number >= INT32_MIN && number <= INT32_MAX)
    return String(cx, value, out);
  char buffer[400];
  if (std::isnan(number)) std::snprintf(buffer, sizeof(buffer), "nan");
  else if (std::isinf(number)) std::snprintf(buffer, sizeof(buffer), number < 0 ? "-inf" : "inf");
  else std::snprintf(buffer, sizeof(buffer), "%.6f", number);
  JS::RootedString string(cx, JS_NewStringCopyZ(cx, buffer));
  if (!string) return false;
  JS::RootedValue converted(cx, JS::StringValue(string));
  return String(cx, converted, out);
}
}
