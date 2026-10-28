#include "web_workers/options.h"
#include "runtime/runtime.h"
#include "runtime/web_kit.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/StableStringChars.h"
#include <initializer_list>

namespace ruyijs::web_workers {
namespace {
bool Enum(JSContext* cx, JS::HandleValue input, std::initializer_list<const char*> values) {
  if (input.isUndefined()) return true;
  JS::RootedString string(cx, JS::ToString(cx, input));
  std::string text;
  if (!string || !ToUTF8(cx, string, &text)) return false;
  for (const char* value : values) if (text == value) return true;
  return WebKit::Fail(cx, "Invalid Worker option enum value");
}
}
bool ReadWorkerName(JSContext* cx, JS::HandleValue input, std::u16string* name) {
  if (input.isNullOrUndefined()) return true;
  if (!input.isObject()) return WebKit::Fail(cx, "Worker options must be an object");
  JS::RootedObject options(cx, &input.toObject());
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, options, "credentials", &value) ||
      !Enum(cx, value, {"omit", "same-origin", "include"}) ||
      !JS_GetProperty(cx, options, "name", &value)) return false;
  if (!value.isUndefined()) {
    JS::RootedString string(cx, JS::ToString(cx, value));
    JS::AutoStableStringChars chars(cx);
    if (!string || !chars.initTwoByte(cx, string)) return false;
    if (chars.length() > 65536) return WebKit::Fail(cx, "Worker name limit exceeded");
    name->assign(chars.twoByteChars(), chars.length());
  }
  return JS_GetProperty(cx, options, "type", &value) && Enum(cx, value, {"classic", "module"});
}
}
