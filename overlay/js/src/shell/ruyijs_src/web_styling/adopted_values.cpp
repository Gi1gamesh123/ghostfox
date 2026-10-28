/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
// One GC-owned backing; DOM migration identity is read from its original port.
#include "web_styling/adopted.h"
#include "js/Array.h"
#include "js/friend/ErrorMessages.h"

namespace ruyijs::web_styling {
namespace { const AdoptedHandler handler; }
bool AdoptedBacking(JSContext* cx, JS::HandleObject proxy, JS::MutableHandleObject out) {
  State& state = AdoptedOwnerOf(proxy);
  JS::RootedObject scope(cx, &js::GetProxyReservedSlot(proxy, AdoptedScope).toObject()), document(cx);
  out.set(&js::GetProxyReservedSlot(proxy, AdoptedRows).toObject());
  if (!state.scopeDocument || !state.scopeDocument(state.adoptedContext, scope, &document)) return false;
  JS::RootedObject old(cx, &js::GetProxyReservedSlot(proxy, AdoptedDocument).toObject());
  if (js::CheckedUnwrapStatic(old) != js::CheckedUnwrapStatic(document)) {
    if (!JS::SetArrayLength(cx, out, 0) || !JS_WrapObject(cx, &document)) return false;
    js::SetProxyReservedSlot(proxy, AdoptedDocument, JS::ObjectValue(*document));
  }
  return true;
}
bool AdoptedLength(JSContext* cx, JS::HandleObject proxy, uint32_t* count) {
  JS::RootedObject rows(cx);
  return AdoptedBacking(cx, proxy, &rows) && JS::GetArrayLength(cx, rows, count);
}
bool AdoptedSetLength(JSContext* cx, JS::HandleObject proxy, JS::HandleValue value,
    JS::ObjectOpResult& result) {
  uint32_t next, previous; double number;
  if (!JS::ToUint32(cx, value, &next) || !JS::ToNumber(cx, value, &number)) return false;
  if (next != number) {
    JS_ReportErrorNumberASCII(cx, js::GetErrorMessage, nullptr, JSMSG_BAD_INDEX); return false;
  }
  JS::RootedObject rows(cx);
  if (!AdoptedBacking(cx, proxy, &rows) || !JS::GetArrayLength(cx, rows, &previous)) return false;
  if (next > previous) return result.failBadArrayLength();
  return JS::SetArrayLength(cx, rows, next) && result.succeed();
}
bool AdoptedPut(JSContext* cx, JS::HandleObject proxy, uint32_t index, JS::HandleValue value,
    JS::ObjectOpResult& result) {
  JS::RootedObject rows(cx), sheet(cx);
  uint32_t count;
  if (!AdoptedBacking(cx, proxy, &rows) || !JS::GetArrayLength(cx, rows, &count)) return false;
  if (index > count) return result.failBadIndex();
  if (!CssReceiver(cx, value, &kSheetClass, &sheet)) return false;
  JS::RootedValue constructorDocument(cx, JS::GetReservedSlot(sheet, SheetConstructorDocument));
  JS::RootedObject current(cx, &js::GetProxyReservedSlot(proxy, AdoptedDocument).toObject());
  if (!SheetDataOf(sheet).constructed || !constructorDocument.isObject() ||
      js::CheckedUnwrapStatic(&constructorDocument.toObject()) != js::CheckedUnwrapStatic(current))
    return AdoptedOwnerOf(proxy).Throw("NotAllowedError", "Sheet requires the scope's constructor document");
  // Validate before replacement; no parallel C++ array can become inconsistent.
  JS::RootedValue stored(cx, JS::ObjectValue(*sheet));
  if (!JS_WrapValue(cx, &stored) || !JS_SetElement(cx, rows, index, stored)) return false;
  return result.succeed();
}
bool NewAdoptedArray(State& state, JS::HandleObject scope, JS::MutableHandleObject out) {
  JSContext* cx = state.cx;
  JS::RootedObject target(cx, JS::NewArrayObject(cx, 0)), rows(cx, JS::NewArrayObject(cx, 0)), document(cx);
  if (!target || !rows || !JS_SetPrototype(cx, rows, nullptr) ||
      !state.scopeDocument || !state.scopeDocument(state.adoptedContext, scope, &document) ||
      !JS_WrapObject(cx, &document)) return false;
  JS::RootedValue value(cx, JS::ObjectValue(*target));
  JS::RootedObject prototype(cx);
  if (!JS_GetPrototype(cx, target, &prototype)) return false;
  js::ProxyOptions options; options.setClass(&kAdoptedClass);
  out.set(js::NewProxyObject(cx, &handler, value, prototype, options));
  if (!out) return false;
  js::SetProxyReservedSlot(out, AdoptedOwner, JS::PrivateValue(&state));
  js::SetProxyReservedSlot(out, AdoptedScope, JS::ObjectValue(*scope));
  js::SetProxyReservedSlot(out, AdoptedDocument, JS::ObjectValue(*document));
  js::SetProxyReservedSlot(out, AdoptedRows, JS::ObjectValue(*rows));
  return true;
}
}
