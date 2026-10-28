#include "runtime/random_control.h"
#include "runtime/resource_schema.h"
#include "jsapi.h"
#include "js/CharacterEncoding.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "vm/JSContext.h"
#include "vm/Realm.h"
#include <cstdint>

namespace ruyijs {
namespace {
bool StateWord(JSContext* cx, JS::HandleObject request, const char* name, uint64_t* word) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, request, name, &value)) return false;
  if (!value.isString() || JS_GetStringLength(value.toString()) != 16) {
    JS_ReportErrorASCII(cx, "random state words must be 16 hexadecimal characters");
    return false;
  }
  JS::RootedString string(cx, value.toString());
  JS::UniqueChars text = JS_EncodeStringToUTF8(cx, string);
  if (!text) return false;
  uint64_t parsed = 0;
  for (size_t index = 0; index < 16; ++index) {
    const unsigned char c = text[index];
    const unsigned digit = c >= '0' && c <= '9' ? c - '0'
        : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 16;
    if (digit == 16) {
      JS_ReportErrorASCII(cx, "random state words must contain only hexadecimal digits");
      return false;
    }
    parsed = (parsed << 4) | digit;
  }
  *word = parsed;
  return true;
}
}

bool ConfigureRandom(JSContext* cx, JS::HandleObject page, JS::HandleObject request,
                     bool frozen, JS::MutableHandleValue result) {
  if (frozen) {
    JS_ReportErrorASCII(cx, "random state configuration is closed after page execution");
    return false;
  }
  uint64_t first = 0, second = 0;
  if (!resource_wire::Fields(cx, request, {"id", "op", "state0", "state1"}) ||
      !StateWord(cx, request, "state0", &first) || !StateWord(cx, request, "state1", &second)) return false;
  if (!first && !second) {
    JS_ReportErrorASCII(cx, "random state words must not both be zero");
    return false;
  }
  {
    JSAutoRealm realm(cx, page);
    cx->realm()->getOrCreateRandomNumberGenerator().setState(first, second);
  }
  result.setBoolean(true);
  return true;
}
}
