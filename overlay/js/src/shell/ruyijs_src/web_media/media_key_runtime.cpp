#include "web_media/media_key_internal.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::web_media {
bool PromiseValue(JSContext* cx, JS::HandleValue value, JS::MutableHandleValue out) {
  JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!promise || !JS::ResolvePromise(cx, promise, value)) return false;
  out.setObject(*promise); return true;
}
bool PromiseUndefined(JSContext* cx, JS::MutableHandleValue out) {
  return PromiseValue(cx, JS::UndefinedHandleValue, out);
}
bool EmptyIterator(JSContext* cx, const char* method, JS::MutableHandleValue out) {
  JS::RootedObject array(cx, JS::NewArrayObject(cx, 0));
  JS::RootedValue fn(cx);
  if (!array || !JS_GetProperty(cx, array, method, &fn)) return false;
  JS::RootedValue thisv(cx, JS::ObjectValue(*array));
  return JS::Call(cx, thisv, fn, JS::HandleValueArray::empty(), out);
}
}
