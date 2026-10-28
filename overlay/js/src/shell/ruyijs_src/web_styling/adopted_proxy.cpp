/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "web_styling/adopted.h"
#include "js/Array.h"
#include "jsfriendapi.h"

namespace ruyijs::web_styling {
const char AdoptedHandler::family = 0;
const JSClass kAdoptedClass = PROXY_CLASS_DEF("Array", JSCLASS_HAS_RESERVED_SLOTS(4));
bool AdoptedKey(JSContext* cx, JS::HandleId id, uint32_t* index, bool* indexed, bool* length) {
  *length = false; *indexed = id.isInt();
  if (*indexed) { *index = uint32_t(id.toInt()); return true; }
  if (!id.isString()) return true;
  JS::RootedString text(cx, id.toString());
  if (!detail::AsciiEquals(cx, text, "length", length)) return false;
  JSLinearString* linear = JS_EnsureLinearString(cx, text);
  if (!linear) return false;
  *indexed = js::StringIsArrayIndex(linear, index) && *index != UINT32_MAX;
  return true;
}
bool AdoptedHandler::get(JSContext* cx, JS::HandleObject proxy, JS::HandleValue receiver,
    JS::HandleId id, JS::MutableHandleValue out) const {
  uint32_t index = 0, count; bool indexed, length;
  if (!AdoptedKey(cx, id, &index, &indexed, &length)) return false;
  JS::RootedObject rows(cx);
  if (!AdoptedBacking(cx, proxy, &rows) || !JS::GetArrayLength(cx, rows, &count)) return false;
  if (length) { out.setNumber(count); return true; }
  if (indexed) {
    if (index >= count) { out.setUndefined(); return true; }
    return JS_GetElement(cx, rows, index, out);
  }
  return ForwardingProxyHandler::get(cx, proxy, receiver, id, out);
}
bool AdoptedHandler::getOwnPropertyDescriptor(JSContext* cx, JS::HandleObject proxy,
    JS::HandleId id, JS::MutableHandle<mozilla::Maybe<JS::PropertyDescriptor>> out) const {
  uint32_t index = 0, count; bool indexed, length;
  if (!AdoptedKey(cx, id, &index, &indexed, &length) ||
      !AdoptedLength(cx, proxy, &count)) return false;
  out.reset();
  JS::RootedValue value(cx, JS::NumberValue(count));
  if (length) {
    out.set(mozilla::Some(JS::PropertyDescriptor::Data(value, {JS::PropertyAttribute::Writable})));
    return true;
  }
  if (indexed) {
    if (index >= count) return true;
    JS::RootedObject rows(cx);
    if (!AdoptedBacking(cx, proxy, &rows) || !JS_GetElement(cx, rows, index, &value)) return false;
    out.set(mozilla::Some(JS::PropertyDescriptor::Data(value,
        {JS::PropertyAttribute::Writable, JS::PropertyAttribute::Enumerable,
         JS::PropertyAttribute::Configurable})));
    return true;
  }
  return ForwardingProxyHandler::getOwnPropertyDescriptor(cx, proxy, id, out);
}
bool AdoptedHandler::has(JSContext* cx, JS::HandleObject proxy, JS::HandleId id, bool* out) const {
  uint32_t index = 0, count; bool indexed, length;
  if (!AdoptedKey(cx, id, &index, &indexed, &length)) return false;
  if (length) { *out = true; return true; }
  if (indexed) {
    if (!AdoptedLength(cx, proxy, &count)) return false;
    *out = index < count; return true;
  }
  return ForwardingProxyHandler::has(cx, proxy, id, out);
}
bool AdoptedHandler::ownPropertyKeys(JSContext* cx, JS::HandleObject proxy,
    JS::MutableHandleIdVector out) const {
  uint32_t count;
  if (!AdoptedLength(cx, proxy, &count)) return false;
  for (uint32_t i = 0; i < count; ++i) {
    JS::RootedId id(cx);
    if (!JS_IndexToId(cx, i, &id) || !out.append(id)) return false;
  }
  return ForwardingProxyHandler::ownPropertyKeys(cx, proxy, out);
}
bool AdoptedHandler::preventExtensions(JSContext*, JS::HandleObject,
    JS::ObjectOpResult& result) const { return result.failCantPreventExtensions(); }
}
