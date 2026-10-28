#include "web_styling/cssom.h"
#include "js/Exception.h"
#include "js/Promise.h"
#include "js/Realm.h"
#include "jsfriendapi.h"

namespace ruyijs::web_styling {
namespace {
bool RejectPending(JSContext* cx, JS::HandleObject promise) {
  if (JS_IsThrowingOutOfMemory(cx) || !JS_IsExceptionPending(cx)) return false;
  JS::RootedValue error(cx);
  if (!JS_GetPendingException(cx, &error)) return false;
  JS_ClearPendingException(cx);
  return JS::RejectPromise(cx, promise, error);
}
void ClearPending(JS::HandleObject sheet) {
  SheetDataOf(sheet).locked = false;
  JS::SetReservedSlot(sheet, SheetPendingPromise, JS::UndefinedValue());
  JS::SetReservedSlot(sheet, SheetPendingText, JS::UndefinedValue());
}
bool Task(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject sheet(cx, &js::GetFunctionNativeReserved(&args.callee(), 0).toObject());
  JSAutoRealm realm(cx, sheet);
  JS::RootedObject promise(cx, &JS::GetReservedSlot(sheet, SheetPendingPromise).toObject());
  JS::RootedValue input(cx, JS::GetReservedSlot(sheet, SheetPendingText));
  std::string text;
  bool ok = CssString(cx, input, &text) && ReplaceRuleRows(cx, sheet, text);
  ClearPending(sheet);
  JS::RootedValue result(cx, JS::ObjectValue(*sheet));
  if (!(ok ? JS::ResolvePromise(cx, promise, result) : RejectPending(cx, promise))) return false;
  args.rval().setUndefined();return true;
}
bool Allowed(State& state, JS::HandleObject sheet) {
  const auto& cache = SheetDataOf(sheet);
  if (!cache.constructed || cache.locked)
    return state.Throw("NotAllowedError", "Sheet replacement is not allowed");
  return true;
}
bool Post(State& state, JS::HandleObject sheet, JS::HandleObject promise, const std::string& text) {
  JSContext* cx = state.cx;
  if (!state.postTask) return WebKit::Fail(cx, "CSSOM task owner is not bound");
  JS::RootedString input(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(text.data(), text.size())));
  JS::RootedFunction fn(cx, js::NewFunctionWithReserved(cx, Task, 0, 0, nullptr));
  if (!input || !fn) return false;
  JS::RootedObject callback(cx, JS_GetFunctionObject(fn));
  js::SetFunctionNativeReserved(callback, 0, JS::ObjectValue(*sheet));
  JS::SetReservedSlot(sheet, SheetPendingText, JS::StringValue(input));
  JS::SetReservedSlot(sheet, SheetPendingPromise, JS::ObjectValue(*promise));
  SheetDataOf(sheet).locked = true;
  if (state.postTask(state.taskContext, callback)) return true;
  ClearPending(sheet);return false;
}
}
bool SheetReplace(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  bool asynchronous = WebKit::Slot(args) != 0;
  JS::RootedObject self(cx);
  if (!CssReceiver(cx, args.thisv(), &kSheetClass, &self)) {
    if (!asynchronous) return false;
    JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
    if (!promise || !RejectPending(cx, promise)) return false;
    args.rval().setObject(*promise);return true;
  }
  State& state = SheetOwnerOf(self);
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, state.page);
    JS::RootedObject promise(cx, asynchronous ? JS::NewPromiseObject(cx, nullptr) : nullptr);
    if (asynchronous && !promise) return false;
    JS::RootedValue input(cx, args.get(0));std::string text;
    bool ok = JS_WrapValue(cx, &input) && (argc ? CssString(cx, input, &text) :
        WebKit::Fail(cx, "Sheet replacement requires text"));
    ok = ok && Allowed(state, self);
    if (!asynchronous) {
      if (!ok || !ReplaceRuleRows(cx, self, text)) return false;
      args.rval().setUndefined();return true;
    }
    ok = ok && Post(state, self, promise, text);
    if (!ok && !RejectPending(cx, promise)) return false;
    result.setObject(*promise);
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result);return true;
}
}  // namespace ruyijs::web_styling
