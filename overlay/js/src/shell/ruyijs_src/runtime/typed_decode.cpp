#include "runtime/typed_decode.h"
#include "runtime/runtime.h"
#include "runtime/pipe.h"
#include <cmath>
#include <limits>
#include "jsapi.h"
#include "js/BigInt.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"

namespace ruyijs {
static bool Invalid(JSContext* cx) {
  JS_ReportErrorASCII(cx, "Invalid typed protocol value");
  return false;
}

bool TypedDecoder::Text(JS::HandleValue value, std::string* text) {
  if (!value.isString()) return Invalid(cx_);
  JS::RootedString string(cx_, value.toString());
  if (JS_GetStringLength(string) > Pipe::kMaxFrame ||
      !ToUTF8(cx_, string, text)) return Invalid(cx_);
  if (text->size() > Pipe::kMaxFrame - textBytes_)
    return Invalid(cx_);
  textBytes_ += text->size();
  return true;
}

bool TypedDecoder::Decode(JS::HandleValue encoded,
                           JS::MutableHandleValue decoded, size_t depth) {
  if (++nodes_ > 100000 || depth > 64 || !encoded.isObject())
    return Invalid(cx_);
  JS::RootedObject object(cx_, &encoded.toObject());
  JS::RootedValue type(cx_), value(cx_);
  std::string kind;
  if (!JS_GetProperty(cx_, object, "type", &type) || !Text(type, &kind) ||
      !JS_GetProperty(cx_, object, "value", &value)) return false;
  if (kind == "undefined") { decoded.setUndefined(); return true; }
  if (kind == "null") { decoded.setNull(); return true; }
  if (kind == "boolean") {
    if (!value.isBoolean()) return Invalid(cx_);
    decoded.set(value); return true;
  }
  if (kind == "string") {
    std::string text;
    if (!Text(value, &text)) return false;
    decoded.set(value); return true;
  }
  if (kind == "number") {
    if (value.isNumber() && std::isfinite(value.toNumber())) {
      decoded.set(value); return true;
    }
    std::string number;
    if (!Text(value, &number)) return false;
    if (number == "NaN") decoded.setDouble(std::numeric_limits<double>::quiet_NaN());
    else if (number == "Infinity") decoded.setDouble(std::numeric_limits<double>::infinity());
    else if (number == "-Infinity") decoded.setDouble(-std::numeric_limits<double>::infinity());
    else if (number == "-0") decoded.setDouble(-0.0);
    else return Invalid(cx_);
    return true;
  }
  if (kind == "bigint") {
    std::string text;
    if (!Text(value, &text) || text.empty()) return Invalid(cx_);
    size_t first = text[0] == '-' ? 1 : 0;
    if (first == text.size() ||
        (text[first] == '0' && text.size() - first > 1)) return Invalid(cx_);
    for (size_t i = first; i < text.size(); ++i)
      if (text[i] < '0' || text[i] > '9') return Invalid(cx_);
    JS::Rooted<JS::BigInt*> number(cx_, JS::SimpleStringToBigInt(cx_,
        mozilla::Span<const char>(text.data(), text.size()), 10));
    if (!number) return false;
    decoded.setBigInt(number); return true;
  }
  if (kind == "array" || kind == "object")
    return Container(value, kind == "array", depth, decoded);
  return Invalid(cx_);
}

bool DecodeTyped(JSContext* cx, JS::HandleValue encoded,
                  JS::MutableHandleValue decoded) {
  return TypedDecoder(cx).Decode(encoded, decoded);
}
}
