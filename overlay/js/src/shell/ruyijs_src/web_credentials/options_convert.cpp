// Generic WebIDL conversions for the JSON option dictionaries. Everything a
// script can observe (getters, iterators, toString) runs in member order.
#include "web_credentials/options_convert.h"
#include "domain/credentials/base64url.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/ArrayBuffer.h"
#include "js/Conversions.h"
#include "js/ForOfIterator.h"
#include "js/PropertyAndElement.h"
#include "mozilla/Range.h"
#include <cstring>

namespace ruyijs::web_credentials {
bool Converter::MissingArgument() {
  return ThrowType(cx, prefix + u"At least 1 argument required, but only 0 passed");
}
bool Converter::Dictionary(JS::HandleValue value, const std::u16string& where,
                           JS::MutableHandleObject out) {
  out.set(nullptr);
  if (value.isNullOrUndefined()) return true;
  if (!value.isObject()) return ThrowType(cx, prefix + where + u" can't be converted to a dictionary.");
  out.set(&value.toObject());
  return true;
}
bool Converter::Member(JS::HandleObject dictionary, const char* name, JS::MutableHandleValue out) {
  out.setUndefined();
  return !dictionary || JS_GetProperty(cx, dictionary, name, out);
}
bool Converter::Required(JS::HandleValue value, const char16_t* member, const char16_t* dictionary) {
  if (!value.isUndefined()) return true;
  return ThrowType(cx, prefix + u"Missing required '" + member + u"' member of " + dictionary + u".");
}
bool Converter::Text(JS::MutableHandleValue value) {
  JSString* text = JS::ToString(cx, value);
  if (!text) return false;
  value.setString(text);
  return true;
}
bool Converter::Text(JS::HandleValue value, std::u16string* out) {
  JS::RootedString text(cx, JS::ToString(cx, value));
  if (!text) return false;
  out->resize(JS_GetStringLength(text));
  return JS_CopyStringChars(cx, mozilla::Range<char16_t>(out->data(), out->size()), text);
}
bool Converter::Sequence(JS::HandleValue value, const std::u16string& where,
                         JS::MutableHandleValueVector items) {
  JS::ForOfIterator iterator(cx);
  if (!value.isObject() || !iterator.init(value, JS::ForOfIterator::AllowNonIterable)) {
    if (value.isObject()) return false;
    return ThrowType(cx, prefix + where + u" can't be converted to a sequence.");
  }
  if (!iterator.valueIsIterable())
    return ThrowType(cx, prefix + where + u" can't be converted to a sequence.");
  JS::RootedValue item(cx);
  for (bool done = false;;) {
    if (!iterator.next(&item, &done)) return false;
    if (done) return true;
    if (!items.append(item)) return false;
  }
}
bool Converter::TextSequence(JS::MutableHandleValue value, const std::u16string& where) {
  JS::RootedValueVector items(cx);
  JS::RootedObject result(cx, Array());
  if (!result || !Sequence(value, where, &items)) return false;
  for (size_t i = 0; i < items.length(); ++i)
    if (!Text(items[i]) || !Push(result, items[i])) return false;
  value.setObject(*result);
  return true;
}
bool Converter::UnsignedLong(JS::MutableHandleValue value) {
  uint32_t number = 0;
  if (!JS::ToUint32(cx, value, &number)) return false;
  value.setNumber(number);
  return true;
}
bool Converter::Enumeration(JS::HandleValue value, const char16_t* member, const char16_t* dictionary,
                            const char16_t* enumeration, std::initializer_list<const char16_t*> values) {
  std::u16string text;
  if (!Text(value, &text)) return false;
  for (const char16_t* known : values) if (text == known) return true;
  return ThrowType(cx, prefix + u"'" + text + u"' (value of '" + member + u"' member of " + dictionary +
                   u") is not a valid value for enumeration " + enumeration + u".");
}
bool Converter::Decoded(JS::HandleValue text, const char16_t* what, JS::MutableHandleValue out) {
  std::u16string input;
  std::string bytes;
  if (!Text(text, &input)) return false;
  if (!domain::credentials::DecodeBase64Url(input, &bytes))
    return ThrowDomException(state, prefix + u"could not decode " + what + u" as urlsafe base64",
                             "EncodingError");
  return Buffer(bytes, out);
}

JSObject* Converter::Array() { return JS::NewArrayObject(cx, 0); }
bool Converter::Push(JS::HandleObject array, JS::HandleValue value) {
  uint32_t length = 0;
  return JS::GetArrayLength(cx, array, &length) && JS_SetElement(cx, array, length, value);
}
bool Converter::Set(JS::HandleObject object, const char* name, JS::HandleValue value) {
  return JS_DefineProperty(cx, object, name, value, JSPROP_ENUMERATE);
}
bool Converter::Literal(const char16_t* text, JS::MutableHandleValue out) {
  JSString* string = JS_NewUCStringCopyZ(cx, text);
  if (!string) return false;
  out.setString(string);
  return true;
}
bool Converter::Buffer(const std::string& bytes, JS::MutableHandleValue out) {
  JS::RootedObject buffer(cx, JS::NewArrayBuffer(cx, bytes.size()));
  if (!buffer) return false;
  {
    JS::AutoCheckCannotGC nogc;
    bool shared = false;
    uint8_t* data = JS::GetArrayBufferData(buffer, &shared, nogc);
    if (!bytes.empty()) std::memcpy(data, bytes.data(), bytes.size());
  }
  out.setObject(*buffer);
  return true;
}
}  // namespace ruyijs::web_credentials
