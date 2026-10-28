// Nested profile values: the Object and Map container kinds a group field can
// declare. An Object is a fixed member list; a Map is a caller-named table
// whose elements are a scalar or a record of members. The bounds are the flat
// field bounds: UTF-8 string length, finite numbers, integral integers and a
// capped entry count, so one profile can never turn into an unbounded tree.
#include "browser_environment/schema.h"
#include "runtime/runtime.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "jsapi.h"
#include <cmath>
#include <limits>

namespace ruyijs::browser_environment {
namespace {
constexpr size_t kMaxEntries = 512;
constexpr size_t kMaxKeyLength = 128;

bool Key(JSContext* cx, JS::HandleId id, std::string* name) {
  JS::RootedValue value(cx);
  if (!JS_IdToValue(cx, id, &value)) return false;
  JS::RootedString text(cx, JS::ToString(cx, value));
  return text && ToUTF8(cx, text, name);
}
// One nested scalar. Same conversion rules as a flat field of that type.
bool Scalar(JSContext* cx, JS::HandleValue value, const Field& descriptor, FieldValue* out) {
  const ValueType type = descriptor.type;
  if (type == ValueType::String) {
    if (!Text(cx, value, &out->text)) return false;
    return !descriptor.allowed || Allowed(out->text, descriptor.allowed) ||
           Error(cx, "Environment field is not an allowed value", JSEXN_RANGEERR);
  }
  if (type == ValueType::Boolean) {
    if (!value.isBoolean()) return Error(cx, "Environment field must be boolean");
    out->boolean = value.toBoolean();
    return true;
  }
  const double number = value.isNumber() ? value.toNumber() : 0;
  if (type == ValueType::Integer) {
    if (!value.isNumber() || !std::isfinite(number) || number != std::floor(number) ||
        number < std::numeric_limits<int32_t>::min() ||
        number > std::numeric_limits<int32_t>::max())
      return Error(cx, "Environment integer is outside its valid range", JSEXN_RANGEERR);
    out->integer = int32_t(number);
    return true;
  }
  if (!value.isNumber() || !std::isfinite(number))
    return Error(cx, "Environment field must be a finite number");
  out->number = number;
  return true;
}
}  // namespace

bool Members(JSContext* cx, JS::HandleObject object, const Field* fields,
             size_t count, bool required, FieldValue* result) {
  JS::Rooted<JS::IdVector> keys(cx, JS::IdVector(cx));
  if (!JS_Enumerate(cx, object, &keys)) return false;
  if (keys.length() > kMaxEntries)
    return Error(cx, "Environment object has too many members", JSEXN_RANGEERR);
  result->items.assign(count, FieldValue());
  for (size_t key = 0; key < keys.length(); ++key) {
    std::string name;
    if (!Key(cx, keys[key], &name)) return false;
    size_t index = 0;
    while (index < count && name != fields[index].name) ++index;
    if (index == count) return Error(cx, "Unknown environment profile field");
    JS::RootedValue value(cx);
    if (!JS_GetPropertyById(cx, object, keys[key], &value)) return false;
    if (!Scalar(cx, value, fields[index], &result->items[index])) return false;
    result->items[index].present = true;
  }
  if (required) {
    for (size_t index = 0; index < count; ++index)
      if (!result->items[index].present)
        return Error(cx, "Environment object needs every declared member");
  }
  return true;
}

bool MapEntries(JSContext* cx, JS::HandleObject object, const Field& descriptor,
                FieldValue* result) {
  JS::Rooted<JS::IdVector> keys(cx, JS::IdVector(cx));
  if (!JS_Enumerate(cx, object, &keys)) return false;
  if (keys.length() > kMaxEntries)
    return Error(cx, "Environment map has too many entries", JSEXN_RANGEERR);
  for (size_t key = 0; key < keys.length(); ++key) {
    std::string name;
    if (!Key(cx, keys[key], &name)) return false;
    if (name.empty() || name.size() > kMaxKeyLength)
      return Error(cx, "Environment map key has invalid length", JSEXN_RANGEERR);
    JS::RootedValue value(cx);
    if (!JS_GetPropertyById(cx, object, keys[key], &value)) return false;
    FieldValue item;
    if (descriptor.members) {
      bool array = false;
      if (!value.isObject() || !JS::IsArrayObject(cx, value, &array))
        return Error(cx, "Environment map element must be an object");
      if (array) return Error(cx, "Environment map element must not be an array");
      JS::RootedObject nested(cx, &value.toObject());
      if (!Members(cx, nested, descriptor.members, descriptor.count, true, &item))
        return false;
    } else {
      // A scalar element: the Map declares its element type in `value`.
      Field element = descriptor;
      element.type = descriptor.value;
      element.members = nullptr;
      element.count = 0;
      if (!Scalar(cx, value, element, &item)) return false;
    }
    result->strings.push_back(std::move(name));
    result->items.push_back(std::move(item));
  }
  return true;
}
}  // namespace ruyijs::browser_environment
