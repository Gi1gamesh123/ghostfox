#include "execution_scheduling/state.h"
#include "runtime/exception_capture.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/Exception.h"
#include "js/Promise.h"
#include "js/Realm.h"

namespace ruyijs::execution_scheduling {
static bool InvokeMicrotask(JSContext* cx, unsigned argc, JS::Value* vp) {
  const auto args = JS::CallArgsFromVp(argc, vp);
  auto* s = static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  JS::RootedValue callback(cx, js::GetFunctionNativeReserved(&args.callee(), 1));
  js::SetFunctionNativeReserved(&args.callee(), 1, JS::UndefinedValue());
  --s->microPending;
  bool ok = true;
  // A microtask queued by a document that has since been retired is dropped.
  if (callback.isObject() && s->Retired(GlobalOf(&callback.toObject()))) {
    args.rval().setUndefined();
    return true;
  }
  if (s->microExecuted >= 100000) ok = Error(cx, "Native microtask execution limit exceeded", true);
  else {
    ++s->microExecuted;
    JS::RootedValue result(cx);
    ok = JS::Call(cx, JS::UndefinedHandleValue, callback, JS::HandleValueArray::empty(), &result);
  }
  if (!ok) {
    CaptureOperationFailure(cx);
    JS_ClearPendingException(cx);
  }
  // Returning undefined also prevents Promise resolution/thenable assimilation.
  args.rval().setUndefined();
  return true;
}
bool EnqueueMicrotask(State& s, JS::HandleValue callback) {
  if (!callback.isObject() || !JS::IsCallable(&callback.toObject()))
    return Error(s.cx, "queueMicrotask requires a callable callback");
  if (s.microPending >= 100000 || s.microEnqueued >= 100000)
    return Error(s.cx, "Native microtask enqueue limit exceeded", true);
  JS::RootedFunction fn(s.cx, js::NewFunctionWithReserved(s.cx, InvokeMicrotask, 0, 0, nullptr));
  if (!fn) return false;
  JS::RootedObject trampoline(s.cx, JS_GetFunctionObject(fn));
  js::SetFunctionNativeReserved(trampoline, 0, JS::PrivateValue(&s));
  js::SetFunctionNativeReserved(trampoline, 1, callback);
  if (!JS::AddPromiseReactions(s.cx, s.fulfilled, trampoline, nullptr)) return false;
  ++s.microPending;
  ++s.microEnqueued;
  return true;
}
bool Microtask(JSContext* cx, unsigned argc, JS::Value* vp) {
  const auto args = JS::CallArgsFromVp(argc, vp);
  State* s = Receiver(cx, args);
  if (!s) return false;
  s->Observe(s->GlobalInterface(), "queueMicrotask", int(ApiAccess::Call));
  if (!argc) return Error(cx, "queueMicrotask requires a callable callback");
  if (s->Retired(GlobalOf(&args.callee()))) { args.rval().setUndefined(); return true; }
  JS::RootedValue callback(cx, args[0]);
  // The resolved promise and its trampoline belong to the scheduling owner's
  // realm, so a call from an iframe realm delivers through the same queue
  // instead of opening a second one.
  JSAutoRealm realm(s->cx, s->page);
  if (!EnqueueMicrotask(*s, callback)) return false;
  args.rval().setUndefined();
  return true;
}
bool Realm::QueueMicrotask(JS::HandleObject callable) {
  // The trampoline and the resolved promise belong to the scheduling
  // owner's realm, so a caller in another realm (an iframe's observer)
  // still delivers through this one queue instead of opening its own.
  JSAutoRealm realm(state_->cx, state_->page);
  JS::RootedValue callback(state_->cx, JS::ObjectValue(*callable));
  return EnqueueMicrotask(*state_, callback);
}
}


