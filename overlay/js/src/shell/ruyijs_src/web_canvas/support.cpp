// The Document Model owns the canvas element; this domain reads and writes its
// reflected width/height through the element's own public DOM methods, so the
// content attribute stays the single source of truth.
#include "web_canvas/internal.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

namespace ruyijs::web_canvas::detail {
namespace {
bool IsSpace(char16_t ch) {
  return ch == u' ' || ch == u'\t' || ch == u'\n' || ch == u'\f' || ch == u'\r';
}
}  // namespace

bool IsCanvas(State& state, JS::HandleValue value) {
  if (!value.isObject()) return false;
  JSContext* cx = state.cx;
  JS::RootedObject object(cx, &value.toObject());
  JS::RootedObject target(cx, state.canvasPrototype);
  while (object) {
    if (object == target) return true;
    JS::RootedObject proto(cx);
    if (!JS_GetPrototype(cx, object, &proto)) return false;
    object = proto;
  }
  return false;
}

bool CallMethod(JSContext* cx, JS::HandleObject receiver, const char* name,
          JS::HandleValueArray args, JS::MutableHandleValue out) {
  JS::RootedValue method(cx);
  if (!JS_GetProperty(cx, receiver, name, &method) || !method.isObject() ||
      !JS::IsCallable(&method.toObject()))
    return WebKit::Fail(cx, "canvas requires the element's DOM methods");
  return JS::Call(cx, receiver, method, args, out);
}

bool GetAttribute(State& state, JS::HandleObject element, const char* name,
                  JS::MutableHandleValue out) {
  JSContext* cx = state.cx;
  JS::RootedString key(cx, JS_NewStringCopyZ(cx, name));
  if (!key) return false;
  JS::RootedValueArray<1> args(cx);
  args[0].setString(key);
  return CallMethod(cx, element, "getAttribute", args, out);
}

bool SetAttribute(State& state, JS::HandleObject element, const char* name,
                  JS::HandleString value) {
  JSContext* cx = state.cx;
  JS::RootedString key(cx, JS_NewStringCopyZ(cx, name));
  if (!key) return false;
  JS::RootedValueArray<2> args(cx);
  args[0].setString(key);
  args[1].setString(value);
  JS::RootedValue ignored(cx);
  return CallMethod(cx, element, "setAttribute", args, &ignored);
}

bool ToUnsignedLong(JSContext* cx, JS::HandleValue value, uint32_t* out) {
  double number = 0.0;
  if (!JS::ToNumber(cx, value, &number)) return false;
  if (!std::isfinite(number) || number == 0.0) {
    *out = 0;
    return true;
  }
  const double modulo = 4294967296.0;
  number = std::fmod(std::trunc(number), modulo);
  if (number < 0.0) number += modulo;
  *out = static_cast<uint32_t>(number);
  return true;
}

bool ParseNonNegative(JSContext* cx, JS::HandleString text, uint32_t* out) {
  const size_t length = JS_GetStringLength(text);
  size_t begin = 0;
  size_t end = length;
  auto at = [&](size_t index, char16_t* ch) {
    return JS_GetStringCharAt(cx, text, index, ch);
  };
  char16_t ch = 0;
  while (begin < end && at(begin, &ch) && IsSpace(ch)) ++begin;
  while (end > begin && at(end - 1, &ch) && IsSpace(ch)) --end;
  size_t index = begin;
  if (index < end && at(index, &ch) && ch == u'+') ++index;
  const size_t digits = index;
  uint64_t value = 0;
  for (; index < end && at(index, &ch) && ch >= u'0' && ch <= u'9'; ++index) {
    value = value * 10 + uint64_t(ch - u'0');
    if (value > 0xFFFFFFFFull) value = 0xFFFFFFFFull;
  }
  if (index == digits || index != end) return false;
  *out = static_cast<uint32_t>(value);
  return true;
}

bool CanvasDimension(State& state, JS::HandleObject element, const char* name,
                     uint32_t fallback, uint32_t* out) {
  JSContext* cx = state.cx;
  JS::RootedValue raw(cx);
  if (!GetAttribute(state, element, name, &raw)) return false;
  return ParseDimension(cx, raw, fallback, out);
}

bool ParseDimension(JSContext* cx, JS::HandleValue raw, uint32_t fallback, uint32_t* out) {
  *out = fallback;
  if (raw.isNull() || raw.isUndefined()) return true;
  JS::RootedString text(cx, JS::ToString(cx, raw));
  if (!text) return false;
  uint32_t parsed = fallback;
  if (ParseNonNegative(cx, text, &parsed)) *out = parsed;
  return true;
}

// The bytes of a string, or an empty string when it holds anything outside
// ASCII, which no CSS colour syntax does.
bool StringToAscii(JSContext* cx, JS::HandleString text, std::string* out) {
  out->clear();
  const size_t length = JS_GetStringLength(text);
  std::string bytes(length, char(0));
  for (size_t index = 0; index < length; ++index) {
    char16_t ch = 0;
    if (!JS_GetStringCharAt(cx, text, index, &ch)) return false;
    if (ch > 0x7f || ch == 0) return true;
    bytes[index] = char(ch);
  }
  *out = std::move(bytes);
  return true;
}

bool StringEqualsAscii(JSContext* cx, JS::HandleString text, const char* ascii, bool* match) {
  const size_t length = JS_GetStringLength(text);
  const size_t wanted = std::strlen(ascii);
  *match = false;
  if (length != wanted) return true;
  for (size_t index = 0; index < length; ++index) {
    char16_t ch = 0;
    if (!JS_GetStringCharAt(cx, text, index, &ch)) return false;
    if (ch != char16_t(uint8_t(ascii[index]))) return true;
  }
  *match = true;
  return true;
}

bool StringEqualsAsciiCI(JSContext* cx, JS::HandleString text, const char* ascii, bool* match) {
  auto lower = [](char16_t ch) {
    return (ch >= u'A' && ch <= u'Z') ? char16_t(ch - u'A' + u'a') : ch;
  };
  const size_t length = JS_GetStringLength(text);
  const size_t wanted = std::strlen(ascii);
  *match = false;
  if (length != wanted) return true;
  for (size_t index = 0; index < length; ++index) {
    char16_t ch = 0;
    if (!JS_GetStringCharAt(cx, text, index, &ch)) return false;
    if (lower(ch) != lower(char16_t(uint8_t(ascii[index])))) return true;
  }
  *match = true;
  return true;
}

bool DecimalString(JSContext* cx, uint32_t value, JS::MutableHandleString out) {
  char buffer[16];
  const int written = std::snprintf(buffer, sizeof(buffer), "%u", value);
  if (written <= 0) return false;
  JS::RootedString text(cx, JS_NewStringCopyN(cx, buffer, size_t(written)));
  if (!text) return false;
  out.set(text);
  return true;
}
}  // namespace ruyijs::web_canvas::detail
