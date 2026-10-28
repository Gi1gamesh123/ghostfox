// One validated profile field -> the JS group object an owner port receives.
// Every configure-only group (webgl, webgpu, media, ...) is buffered in State
// and pushed through its owner's Configure, so the object shape a profile used
// and the shape the owner reads are the same, including Object and Map fields.
#include "browser_environment/schema.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"

namespace ruyijs::browser_environment {
namespace {
bool Value(JSContext* cx, const Field& descriptor, const FieldValue& value,
           JS::MutableHandleValue result);
bool Strings(JSContext* cx, const std::vector<std::string>& sources,
             JS::MutableHandleValue result) {
  JS::RootedObject list(cx, JS::NewArrayObject(cx, sources.size()));
  if (!list) return false;
  for (size_t index = 0; index < sources.size(); ++index) {
    JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(
        sources[index].data(), sources[index].size())));
    if (!text || !JS_DefineElement(cx, list, index, text, JSPROP_ENUMERATE))
      return false;
  }
  result.setObject(*list);
  return true;
}
bool Members(JSContext* cx, const Field* fields, size_t count,
             const FieldValue& value, JS::MutableHandleValue result) {
  JS::RootedObject object(cx, JS_NewPlainObject(cx));
  if (!object) return false;
  for (size_t index = 0; index < count && index < value.items.size(); ++index) {
    // Only the members the profile actually supplied: an absent member keeps
    // the owner's default instead of becoming a false/empty override.
    if (!value.items[index].present) continue;
    JS::RootedValue member(cx);
    if (!Value(cx, fields[index], value.items[index], &member) ||
        !JS_DefineProperty(cx, object, fields[index].name, member,
                           JSPROP_ENUMERATE)) return false;
  }
  result.setObject(*object);
  return true;
}
bool Value(JSContext* cx, const Field& descriptor, const FieldValue& value,
           JS::MutableHandleValue result) {
  if (descriptor.type == ValueType::Object)
    return Members(cx, descriptor.members, descriptor.count, value, result);
  if (descriptor.type == ValueType::Map) {
    // A map is one plain object; its keys are caller data, so its element
    // records are written with the declared member names only.
    JS::RootedObject object(cx, JS_NewPlainObject(cx));
    if (!object) return false;
    for (size_t index = 0; index < value.strings.size() && index < value.items.size();
         ++index) {
      JS::RootedValue element(cx);
      // A scalar map (no member list) writes each element as its declared
      // value type; a record map writes the member records.
      Field scalar = descriptor;
      scalar.type = descriptor.value;
      scalar.members = nullptr;
      scalar.count = 0;
      if (!(descriptor.members
                ? Members(cx, descriptor.members, descriptor.count, value.items[index], &element)
                : Value(cx, scalar, value.items[index], &element)) ||
          !JS_DefineProperty(cx, object, value.strings[index].c_str(), element,
                             JSPROP_ENUMERATE))
        return false;
    }
    result.setObject(*object);
    return true;
  }
  if (descriptor.type == ValueType::Strings) return Strings(cx, value.strings, result);
  if (descriptor.type == ValueType::Boolean) {
    result.setBoolean(value.boolean);
    return true;
  }
  if (descriptor.type == ValueType::Number) {
    result.setDouble(value.number);
    return true;
  }
  if (descriptor.type == ValueType::Integer) {
    result.setInt32(value.integer);
    return true;
  }
  JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(
      value.text.data(), value.text.size())));
  if (!text) return false;
  result.setString(text);
  return true;
}
}  // namespace

bool WriteField(JSContext* cx, JS::HandleObject group, const Field& descriptor,
                const FieldValue& value) {
  JS::RootedValue out(cx);
  return Value(cx, descriptor, value, &out) &&
         JS_DefineProperty(cx, group, descriptor.name, out, JSPROP_ENUMERATE);
}

bool MergeGroup(JSContext* cx, JS::MutableHandleObject buffer, Kind kind,
                const Field* fields, size_t count,
                const std::vector<Change>& changes) {
  if (!buffer.get()) {
    JS::RootedObject group(cx, JS_NewPlainObject(cx));
    if (!group) return false;
    buffer.set(group);
  }
  for (const auto& change : changes) {
    if (change.kind != kind || change.field >= count) continue;
    if (!WriteField(cx, buffer, fields[change.field], change.value)) return false;
  }
  return true;
}
}  // namespace ruyijs::browser_environment
