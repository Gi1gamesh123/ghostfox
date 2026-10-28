#include "browser_environment/schema.h"
#include "runtime/runtime.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include <cmath>
#include <limits>

namespace ruyijs::browser_environment {
bool Text(JSContext* cx, JS::HandleValue value, std::string* result) {
  if (!value.isString()) return Error(cx, "Environment field must be a string");
  JS::RootedString text(cx, value.toString());
  size_t length = JS_GetStringLength(text);
  if (length > 16384)
    return Error(cx, "Environment string exceeds 16384 code units", JSEXN_RANGEERR);
  for (size_t index = 0; index < length; ++index) {
    char16_t unit;
    if (!JS_GetStringCharAt(cx, text, index, &unit)) return false;
    if (unit >= 0xd800 && unit <= 0xdbff) {
      if (++index >= length || !JS_GetStringCharAt(cx, text, index, &unit))
        return Error(cx, "Environment string has an unpaired surrogate", JSEXN_RANGEERR);
      if (unit >= 0xdc00 && unit <= 0xdfff) continue;
      return Error(cx, "Environment string has an unpaired surrogate", JSEXN_RANGEERR);
    }
    if (unit >= 0xdc00 && unit <= 0xdfff)
      return Error(cx, "Environment string has an unpaired surrogate", JSEXN_RANGEERR);
  }
  return ToUTF8(cx, text, result);
}
bool Allowed(const std::string& text, const char* allowed) {
  const char* at = allowed;
  while (*at) {
    const char* end = at;
    while (*end && *end != '|') ++end;
    if (text.size() == size_t(end - at) && text.compare(0, text.size(), at, 0, size_t(end - at)) == 0)
      return true;
    at = *end ? end + 1 : end;
  }
  return false;
}
bool ReadField(JSContext* cx, JS::HandleValue value, Kind kind, uint32_t field,
               const Field& descriptor, FieldValue* result) {
  const ValueType type = descriptor.type;
  if (type == ValueType::Object || type == ValueType::Map) {
    bool array = false;
    if (!value.isObject() || !JS::IsArrayObject(cx, value, &array))
      return Error(cx, "Environment field must be an object");
    if (array) return Error(cx, "Environment field must not be an array");
    JS::RootedObject object(cx, &value.toObject());
    return type == ValueType::Object
               ? Members(cx, object, descriptor.members, descriptor.count, false, result)
               : MapEntries(cx, object, descriptor, result);
  }
  if (type == ValueType::String) {
    if (!Text(cx, value, &result->text)) return false;
    // An allow-listed string field (enumerations) is checked here, once.
    if (descriptor.allowed && !Allowed(result->text, descriptor.allowed))
      return Error(cx, "Environment field is not an allowed value", JSEXN_RANGEERR);
    if (kind == Kind::Permissions && result->text != "granted" && result->text != "denied" &&
        result->text != "prompt")
      return Error(cx, "Permission state must be granted, denied or prompt", JSEXN_RANGEERR);
    return kind != Kind::Location || ValidURL(cx, result->text);
  }
  if (type == ValueType::Number) {
    // The geolocation group: optional members (altitude onward) accept null.
    if (value.isNull() && kind == Kind::Geolocation && descriptor.nullable) {
      result->null = true;
      return true;
    }
    if (!value.isNumber() || !std::isfinite(value.toNumber()))
      return Error(cx, "Environment field must be a finite number");
    result->number = value.toNumber();
    // devicePixelRatio is a positive CSS pixel ratio; Firefox never reports 0
    // and no desktop display ratio exceeds 8.
    if (kind == Kind::Viewport && field == kViewportDevicePixelRatio &&
        (result->number <= 0 || result->number > 8))
      return Error(cx, "devicePixelRatio must be greater than 0 and at most 8",
                   JSEXN_RANGEERR);
    // Storage bytes are non-negative; a negative estimate is meaningless.
    if (kind == Kind::Storage && result->number < 0)
      return Error(cx, "Storage byte count must not be negative", JSEXN_RANGEERR);
    if (kind == Kind::Geolocation) {
      const double number = result->number;
      if ((field == 0 && (number < -90 || number > 90)) ||
          (field == 1 && (number < -180 || number > 180)) ||
          ((field == 2 || field == 4 || field == 6 || field == 7) && number < 0) ||
          (field == 5 && (number < 0 || number >= 360)))
        return Error(cx, "Geolocation value is outside its valid range", JSEXN_RANGEERR);
    }
    return true;
  }
  if (type == ValueType::Boolean) {
    if (!value.isBoolean()) return Error(cx, "Environment field must be boolean");
    result->boolean = value.toBoolean();
    return true;
  }
  if (type == ValueType::Integer) {
    if (!value.isNumber()) return Error(cx, "Environment field must be numeric");
    double number = value.toNumber();
    double minimum = kind == Kind::Screen && (field == 4 || field == 5)
        ? std::numeric_limits<int32_t>::min() : 0;
    if (kind == Kind::Navigator && field == 11) minimum = 1;
    if (!std::isfinite(number) || number != std::floor(number) || number < minimum ||
        number > std::numeric_limits<int32_t>::max())
      return Error(cx, "Environment integer is outside its valid range", JSEXN_RANGEERR);
    result->integer = int32_t(number);
    return true;
  }
  bool array = false;
  if (!JS::IsArrayObject(cx, value, &array)) return false;
  if (!array)
    return Error(cx, kind == Kind::Navigator ? "languages must be an array of strings"
                                             : "value must be an array of strings");
  JS::RootedObject list(cx, &value.toObject());
  uint32_t length;
  if (!JS::GetArrayLength(cx, list, &length)) return false;
  // navigator.languages and the speech voice list are lists a page reads as
  // "the set I have", so they need at least one entry; the extension and
  // device lists may legitimately be empty (a profile that removes everything).
  const bool needsEntry = kind == Kind::Navigator || kind == Kind::Speech;
  // An installed-font list is a machine's whole family set (hundreds of names).
  const uint32_t limit = kind == Kind::Fonts ? 4096 : 32;
  if (length > limit || (length == 0 && needsEntry))
    return Error(cx, needsEntry ? "languages must contain 1 to 32 entries"
                                : kind == Kind::Fonts ? "value must contain at most 4096 entries"
                                                      : "value must contain at most 32 entries",
                 JSEXN_RANGEERR);
  for (uint32_t index = 0; index < length; ++index) {
    JS::RootedValue item(cx);
    std::string text;
    if (!JS_GetElement(cx, list, index, &item) || !Text(cx, item, &text)) return false;
    if (text.empty() || text.size() > 128)
      return Error(cx, "Language identifier has invalid length", JSEXN_RANGEERR);
    result->strings.push_back(std::move(text));
  }
  return true;
}
}
