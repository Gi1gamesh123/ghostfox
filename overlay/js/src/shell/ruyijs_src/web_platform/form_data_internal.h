#ifndef RUYIJS_WEB_PLATFORM_FORM_DATA_INTERNAL_H
#define RUYIJS_WEB_PLATFORM_FORM_DATA_INTERNAL_H
#include "web_platform/state.h"

#include "runtime/web_kit.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/CallArgs.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include <string>

namespace ruyijs::web_platform {
// The one ordered-list vocabulary the FormData operations share: read an entry,
// read the list length, split an entry into its name/value, append one entry,
// read the argument name and test an entry's name. Both the mutators and the
// queries go through these, so no operation invents its own accessor.
namespace form_data_internal {
inline bool Entry(JSContext* cx, JS::HandleObject list, uint32_t index,
                  JS::MutableHandleObject pair) {
  JS::RootedValue value(cx);
  if (!JS_GetElement(cx, list, index, &value) || !value.isObject()) return false;
  pair.set(&value.toObject());
  return true;
}

inline bool Length(JSContext* cx, JS::HandleObject list, uint32_t* length) {
  bool array = false;
  return JS::IsArrayObject(cx, list, &array) && array &&
         JS::GetArrayLength(cx, list, length);
}

inline bool Pair(JSContext* cx, JS::HandleObject pair, JS::MutableHandleValue name,
                 JS::MutableHandleValue value) {
  return JS_GetElement(cx, pair, 0, name) && JS_GetElement(cx, pair, 1, value);
}

inline bool Push(JSContext* cx, JS::HandleObject list, uint32_t index,
                 JS::HandleObject value) {
  JS::RootedValue item(cx, JS::ObjectValue(*value));
  return JS_SetElement(cx, list, index, item);
}

// The argument name, stored as the string the comparison also uses.
inline bool Name(JSContext* cx, const JS::CallArgs& args,
                 JS::MutableHandleValue name, std::string* key) {
  if (!args.length()) return WebKit::Fail(cx, "FormData requires a name");
  return Convert(cx, args[0], key) && Value(cx, *key, name);
}

inline bool Match(JSContext* cx, JS::HandleObject pair, const std::string& key,
                  bool* match) {
  JS::RootedValue name(cx), value(cx);
  std::string stored;
  if (!Pair(cx, pair, &name, &value) || !Convert(cx, name, &stored)) return false;
  *match = stored == key;
  return true;
}

inline bool NewPair(JSContext* cx, JS::HandleValue name, JS::HandleValue value,
                    JS::MutableHandleObject pair) {
  JS::RootedObject created(cx, JS::NewArrayObject(cx, 2));
  if (!created || !JS_SetElement(cx, created, 0, name) ||
      !JS_SetElement(cx, created, 1, value))
    return false;
  pair.set(created);
  return true;
}
}  // namespace form_data_internal
}  // namespace ruyijs::web_platform
#endif
