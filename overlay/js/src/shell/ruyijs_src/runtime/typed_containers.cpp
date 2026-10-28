#include "runtime/typed_decode.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"

namespace ruyijs {
bool TypedDecoder::Container(JS::HandleValue input, bool array, size_t depth,
                             JS::MutableHandleValue decoded) {
  bool isArray = false;
  if (!JS::IsArrayObject(cx_, input, &isArray)) return false;
  if (!isArray) {
    JS_ReportErrorASCII(cx_, "Typed container must contain an array");
    return false;
  }
  JS::RootedObject entries(cx_, &input.toObject());
  uint32_t length;
  if (!JS::GetArrayLength(cx_, entries, &length)) return false;
  if (length > 100000) {
    JS_ReportErrorASCII(cx_, "Typed container exceeds node limit");
    return false;
  }
  JS::RootedObject result(cx_, array ? JS::NewArrayObject(cx_, length)
                                    : JS_NewPlainObject(cx_));
  if (!result) return false;
  for (uint32_t index = 0; index < length; ++index) {
    JS::RootedValue entry(cx_), value(cx_);
    if (!JS_GetElement(cx_, entries, index, &entry)) return false;
    if (array) {
      if (!Decode(entry, &value, depth + 1) ||
          !JS_DefineElement(cx_, result, index, value, JSPROP_ENUMERATE)) return false;
      continue;
    }
    if (!JS::IsArrayObject(cx_, entry, &isArray)) return false;
    if (!isArray) {
      JS_ReportErrorASCII(cx_, "Typed object entry must be a key/value pair");
      return false;
    }
    JS::RootedObject pair(cx_, &entry.toObject());
    uint32_t pairLength;
    if (!JS::GetArrayLength(cx_, pair, &pairLength)) return false;
    if (pairLength != 2) {
      JS_ReportErrorASCII(cx_, "Typed object entry must have two elements");
      return false;
    }
    JS::RootedValue key(cx_), nested(cx_);
    std::string text;
    if (!JS_GetElement(cx_, pair, 0, &key) || !Text(key, &text) ||
        !JS_GetElement(cx_, pair, 1, &nested)) return false;
    JS::RootedString string(cx_, key.toString());
    JS::RootedId id(cx_);
    bool exists;
    if (!JS_StringToId(cx_, string, &id) ||
        !JS_HasOwnPropertyById(cx_, result, id, &exists)) return false;
    if (exists) {
      JS_ReportErrorASCII(cx_, "Typed object contains duplicate keys");
      return false;
    }
    if (!Decode(nested, &value, depth + 1) ||
        !JS_DefinePropertyById(cx_, result, id, value, JSPROP_ENUMERATE)) return false;
  }
  decoded.setObject(*result);
  return true;
}
}
