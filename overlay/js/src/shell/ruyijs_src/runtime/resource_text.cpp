#include "runtime/resource_schema.h"
#include "runtime/resource_buffer.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/CharacterEncoding.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/StableStringChars.h"
#include "js/String.h"

namespace ruyijs::resource_wire {
using resource_network::Report;
bool Error(JSContext* cx, const char* message, bool range) {
  return Report(cx, {range ? resource_network::ErrorCode::Range :
                            resource_network::ErrorCode::Type, message});
}
bool Object(JSContext* cx, JS::HandleValue value, JS::MutableHandleObject result) {
  bool array = false;
  if (!value.isObject()) return Error(cx, "Resource field must be an object");
  if (!JS::IsArrayObject(cx, value, &array)) return false;
  if (array) return Error(cx, "Resource field must be an object, not an array");
  result.set(&value.toObject());
  return true;
}
bool Text(JSContext* cx, JS::HandleValue value, std::string* output, size_t limit, TextKind kind) {
  if (!value.isString()) return Error(cx, "Resource field must be a string");
  JS::RootedString text(cx, value.toString());
  const size_t length = JS_GetStringLength(text);
  if (length > limit) return Error(cx, "Resource string exceeds its size limit", true);
  JS::AutoStableStringChars chars(cx);
  if (!chars.init(cx, text)) return false;
  auto unit = [&](size_t i) -> char16_t {
    return chars.isLatin1() ? chars.latin1Chars()[i] : chars.twoByteChars()[i];
  };
  for (size_t i = 0; i < length; ++i) {
    char16_t ch = unit(i);
    if (kind != TextKind::UTF8) {
      if (ch > (kind == TextKind::ASCII ? 127 : 255))
        return Error(cx, "Resource string has an invalid code unit");
    } else if (ch >= 0xd800 && ch <= 0xdbff) {
      if (++i >= length || unit(i) < 0xdc00 || unit(i) > 0xdfff)
        return Error(cx, "Resource key has an unpaired surrogate");
    } else if (ch >= 0xdc00 && ch <= 0xdfff)
      return Error(cx, "Resource key has an unpaired surrogate");
  }
  if (kind != TextKind::UTF8) {
    if (!Report(cx, Allocate(output, length))) return false;
    for (size_t i = 0; i < length; ++i) (*output)[i] = char(unit(i));
    return true;
  }
  JSLinearString* linear = JS_EnsureLinearString(cx, text);
  if (!linear) return false;
  const size_t bytes = JS::GetDeflatedUTF8StringLength(linear);
  if (bytes > limit) return Error(cx, "Resource string exceeds its byte limit", true);
  if (!Report(cx, Allocate(output, bytes))) return false;
  JS::DeflateStringToUTF8Buffer(linear, mozilla::Span<char>(output->data(), bytes));
  return true;
}
bool Property(JSContext* cx, JS::HandleObject object, const char* name,
              std::string* output, size_t limit, TextKind kind) {
  JS::RootedValue value(cx);
  return JS_GetProperty(cx, object, name, &value) && Text(cx, value, output, limit, kind);
}
bool Fields(JSContext* cx, JS::HandleObject object, std::initializer_list<const char*> allowed) {
  JS::Rooted<JS::IdVector> keys(cx, JS::IdVector(cx));
  if (!JS_Enumerate(cx, object, &keys)) return false;
  for (size_t i = 0; i < keys.length(); ++i) {
    JS::RootedId id(cx, keys[i]);
    JS::RootedValue key(cx);
    if (!JS_IdToValue(cx, id, &key)) return false;
    JS::RootedString name(cx, JS::ToString(cx, key));
    if (!name) return false;
    bool found = false;
    for (const char* field : allowed) {
      bool equal;
      if (!JS_StringEqualsAscii(cx, name, field, &equal)) return false;
      if (equal) { found = true; break; }
    }
    if (!found) return Error(cx, "Unknown resource field");
  }
  return true;
}
}
