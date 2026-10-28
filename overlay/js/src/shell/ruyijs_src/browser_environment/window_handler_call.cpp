#include "browser_environment/window_handlers.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/ErrorReport.h"
#include "js/Exception.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include <cstring>

namespace ruyijs::browser_environment {
namespace {
bool Call(JSContext* cx, JS::HandleObject store, unsigned slot, JS::HandleObject receiver,
           const JS::HandleValueArray& arguments, JS::MutableHandleValue result) {
  JS::RootedValue function(cx, JS::GetReservedSlot(store, slot));
  JS::RootedValue self(cx, JS::ObjectValue(*receiver));
  return JS::Call(cx, self, function, arguments, result);
}
// Original getters validate the native event brand across realms. Only their
// incompatible-receiver TypeError means "different event"; OOM/termination stay errors.
bool Branded(JSContext* cx, JS::HandleObject store, unsigned slot, JS::HandleObject event,
              JS::MutableHandleValue result, bool* matched) {
  *matched = Call(cx, store, slot, event, JS::HandleValueArray::empty(), result);
  if (*matched) return true;
  JS::RootedValue pending(cx);
  if (JS_IsThrowingOutOfMemory(cx) || !JS_GetPendingException(cx, &pending) || !pending.isObject()) return false;
  JS::RootedObject error(cx, &pending.toObject());
  JS::BorrowedErrorReport report(cx);
  if (!JS_ErrorFromException(cx, error, report) || report->exnType != JSEXN_TYPEERR) return false;
  JS_ClearPendingException(cx); return true;
}
}
bool WindowHandlerResult(JSContext* cx, JS::HandleObject store, unsigned index,
                         JS::HandleObject event, JS::HandleValue function,
                         JS::HandleObject window) {
  const char* type = kWindowHandlerNames[index] + 2;
  bool error = false, before = false;
  JS::RootedValueArray<5> arguments(cx);
  JS::RootedValue beforeValue(cx);
  if (!std::strcmp(type, "error") && !Branded(cx, store, kHandlerErrorMessage, event, arguments[0], &error)) return false;
  if (!std::strcmp(type, "beforeunload") && !Branded(cx, store, kHandlerBeforeUnloadGet, event, &beforeValue, &before)) return false;
  if (error) {
    for (unsigned i = 1; i < 5; ++i)
      if (!Call(cx, store, kHandlerErrorMessage + i, event, JS::HandleValueArray::empty(), arguments[i])) return false;
  } else arguments[0].setObject(*event);
  JS::RootedValue receiver(cx, JS::ObjectValue(*window)), result(cx);
  if (!JS::Call(cx, receiver, function, JS::HandleValueArray::subarray(arguments, 0, error ? 5 : 1), &result)) return false;
  bool cancel = result.isBoolean() && result.toBoolean() == error;
  if (before) {
    cancel = !result.isNullOrUndefined();
    if (cancel) {
      JS::RootedString text(cx, JS::ToString(cx, result));
      JS::RootedValue previous(cx), string(cx);
      if (!text || !Call(cx, store, kHandlerBeforeUnloadGet, event, JS::HandleValueArray::empty(), &previous)) return false;
      string.setString(text);
      if (previous.isString() && JS_GetStringLength(previous.toString()) == 0 &&
          !Call(cx, store, kHandlerBeforeUnloadSet, event, JS::HandleValueArray(string), &previous)) return false;
    }
  }
  if (cancel) return Call(cx, store, kHandlerPrevent, event, JS::HandleValueArray::empty(), &result);
  return true;
}
bool InvokeWindowHandler(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject prototype(cx);
  State* owner = WindowOwner(cx, args, &prototype);
  if (!owner || !args.get(0).isObject()) return false;
  const unsigned index = unsigned(js::GetFunctionNativeReserved(&args.callee(), 1).toInt32());
  JS::RootedValue eventValue(cx, args[0]);
  {
    JSAutoRealm realm(cx, owner->page);
    if (!JS_WrapValue(cx, &eventValue)) return false;
    JS::RootedObject event(cx, &eventValue.toObject());
    JS::RootedObject window(cx, owner->page);
    JS::RootedObject store(cx, &JS::GetReservedSlot(prototype, kWindowHandlerStoreSlot).toObject());
    JS::RootedObject values(cx, &JS::GetReservedSlot(store, kHandlerValues).toObject());
    JS::RootedValue callback(cx);
    if (!JS_GetElement(cx, values, index, &callback)) return false;
    if (callback.isObject() && !WindowHandlerResult(cx, store, index, event, callback, window)) return false;
  }
  args.rval().setUndefined(); return true;
}
}
