/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "web_styling/adopted.h"
#include "js/Array.h"

namespace ruyijs::web_styling {
bool AdoptedHandler::defineProperty(JSContext* cx, JS::HandleObject proxy, JS::HandleId id,
    JS::Handle<JS::PropertyDescriptor> desc, JS::ObjectOpResult& result) const {
  uint32_t index = 0; bool indexed, length;
  if (!AdoptedKey(cx, id, &index, &indexed, &length)) return false;
  if (length || indexed) {
    if (desc.isAccessorDescriptor()) return result.failNotDataDescriptor();
    if ((desc.hasWritable() && !desc.writable()) ||
        (desc.hasConfigurable() && desc.configurable() == length) ||
        (desc.hasEnumerable() && desc.enumerable() == length)) return result.failInvalidDescriptor();
    if (!desc.hasValue()) return result.succeed();
    JS::RootedValue value(cx, desc.value());
    return length ? AdoptedSetLength(cx, proxy, value, result) :
                    AdoptedPut(cx, proxy, index, value, result);
  }
  return ForwardingProxyHandler::defineProperty(cx, proxy, id, desc, result);
}
bool AdoptedHandler::set(JSContext* cx, JS::HandleObject proxy, JS::HandleId id,
    JS::HandleValue value, JS::HandleValue receiver, JS::ObjectOpResult& result) const {
  uint32_t index = 0; bool indexed, length;
  if (!AdoptedKey(cx, id, &index, &indexed, &length)) return false;
  if (length) return AdoptedSetLength(cx, proxy, value, result);
  if (indexed) return AdoptedPut(cx, proxy, index, value, result);
  return ForwardingProxyHandler::set(cx, proxy, id, value, receiver, result);
}
bool AdoptedHandler::delete_(JSContext* cx, JS::HandleObject proxy, JS::HandleId id,
    JS::ObjectOpResult& result) const {
  uint32_t index = 0, count; bool indexed, length;
  if (!AdoptedKey(cx, id, &index, &indexed, &length)) return false;
  if (length) return result.failCantDelete();
  if (indexed) {
    JS::RootedObject rows(cx);
    if (!AdoptedBacking(cx, proxy, &rows) || !JS::GetArrayLength(cx, rows, &count)) return false;
    if (count != index + 1) return result.failBadIndex();
    return JS::SetArrayLength(cx, rows, index) && result.succeed();
  }
  return ForwardingProxyHandler::delete_(cx, proxy, id, result);
}
}
