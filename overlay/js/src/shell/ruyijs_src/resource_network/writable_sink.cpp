// Captured callbacks preserve WebIDL entry failure separately from JS throws.
#include "resource_network/writable_stream.h"
#include "resource_network/state.h"
#include "resource_network/abort.h"
#include "resource_network/native_sink.h"
#include "js/CallAndConstruct.h"
#include "js/Exception.h"
#include "js/Promise.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
CallbackResult SinkEntry(JSContext* cx,JS::HandleObject controller,JS::HandleValue callback,CallbackContext context) {
  if (callback.isUndefined()) return CallbackResult::Success;
  // Same original owner edge used by WritableAbort; no async State pointer.
  JS::RootedObject signal(cx,SlotObject(controller,KSignal));
  auto& owner=*static_cast<State*>(JS::GetReservedSlot(signal,AOwner).toPrivate());
  return CheckCallbackEntry(owner,callback,context);
}
namespace {
bool Invoke(JSContext* cx,JS::HandleObject controller,unsigned slot,JS::HandleValue argument,
            JS::MutableHandleValue result,bool* entered) {
  *entered=false;
  JS::RootedValue callback(cx,JS::GetReservedSlot(controller,slot));
  if (callback.isUndefined()) {result.setUndefined();return true;}
  JS::RootedValue receiver(cx,JS::GetReservedSlot(controller,KSink));
  JS::RootedValueArray<2> args(cx);
  args[0].set(slot==KStart?JS::ObjectValue(*controller):argument.get());
  args[1].setObject(*controller);
  if (!JS_WrapValue(cx,&callback) || !JS_WrapValue(cx,&receiver) ||
      !JS_WrapValue(cx,args[0]) || !JS_WrapValue(cx,args[1])) return false;
  const unsigned count=slot==KClose?0:slot==KWrite?2:1;
  *entered=true;
  return JS::Call(cx,receiver,callback,JS::HandleValueArray::subarray(args,0,count),result);
}
bool Capture(JSContext* cx,JS::MutableHandleObject result) {
  JS::RootedValue reason(cx);
  if (JS_IsThrowingOutOfMemory(cx) || !JS_IsExceptionPending(cx) || !JS_GetPendingException(cx,&reason)) return false;
  JS_ClearPendingException(cx);result.set(JS::CallOriginalPromiseReject(cx,reason));
  return !!result;
}
}
CallbackResult SinkCall(JSContext* cx,JS::HandleObject controller,unsigned slot,JS::HandleValue argument,
                        JS::MutableHandleValue result,CallbackContext context) {
  if (HasNativeSinkAlgorithms(controller)) return InvokeNativeSink(cx,controller,slot,argument,result,context);
  JS::RootedValue callback(cx,JS::GetReservedSlot(controller,slot));
  const auto entry=SinkEntry(cx,controller,callback,context);
  if (entry!=CallbackResult::Success) return entry;
  bool entered=false;
  return CallbackOutcome(Invoke(cx,controller,slot,argument,result,&entered));
}
CallbackResult SinkPromise(JSContext* cx,JS::HandleObject controller,unsigned slot,JS::HandleValue argument,
                           JS::MutableHandleObject result,CallbackContext context) {
  if (HasNativeSinkAlgorithms(controller)) {
    JS::RootedValue value(cx);
    const auto outcome=InvokeNativeSink(cx,controller,slot,argument,&value,context);
    if (outcome!=CallbackResult::Success) return outcome;
    result.set(&value.toObject());return CallbackResult::Success;
  }
  JS::RootedValue callback(cx,JS::GetReservedSlot(controller,slot)),value(cx);
  const auto entry=SinkEntry(cx,controller,callback,context);
  if (entry!=CallbackResult::Success) return entry;
  JS::RootedObject target(cx,controller);
  if (!callback.isUndefined()) {
    target=js::CheckedUnwrapStatic(&callback.toObject());
    if (!target) {Error(cx,"WritableStream callback is unavailable");return CallbackResult::Failure;}
  }
  {
    JSAutoRealm realm(cx,target);
    bool entered=false;
    if (!Invoke(cx,controller,slot,argument,&value,&entered)) {
      if (!entered || !Capture(cx,result)) return CallbackResult::Failure;
    } else {
      result.set(JS::CallOriginalPromiseResolve(cx,value));
      if (!result && !Capture(cx,result)) return CallbackResult::Failure;
    }
  }
  return CallbackOutcome(JS_WrapObject(cx,result));
}
}
