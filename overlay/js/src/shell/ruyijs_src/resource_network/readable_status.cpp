// One terminal state and one disturbance authority for each existing source kind.
#include "resource_network/readable_controller.h"
#include "resource_network/readable_byte.h"
#include "resource_network/state.h"
#include "jsfriendapi.h"
#include "js/Promise.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
void DisturbReadable(JSObject* stream) {
  if (BodyRecord* body=StreamBody(stream)) body->used=true;
  else JS::SetReservedSlot(stream,SDisturbed,JS::BooleanValue(true));
}
bool RejectReads(JSContext* cx,JS::HandleObject stream,JS::HandleValue reason) {
  while (SlotObject(stream,SHead)) {
    JS::RootedObject request(cx,SlotObject(stream,SHead));
    JS::RootedObject promise(cx,SlotObject(request,QTarget));
    const bool native=IsNativeReadCompletion(promise);
    if (native) PopReadRequest(stream);
    if (!RejectReadCompletion(cx,promise,reason)) return false;
    if (!native) PopReadRequest(stream);
  }
  return true;
}
bool ErrorReadable(JSContext* cx,JS::HandleObject stream,JS::HandleValue reason) {
  if (ReadableStatus(stream)!=StreamStatus::Readable) return true;
  {
    JSAutoRealm realm(cx,stream);
    JS::RootedValue stored(cx,reason);
    if (!JS_WrapValue(cx,&stored)) return false;
    JS::SetReservedSlot(stream,SError,stored);
    SetReadableStatus(stream,StreamStatus::Errored);
  }
  if (JSObject* reader=SlotObject(stream,SReader)) {
    JS::RootedObject closed(cx,SlotObject(reader,RClosed));
    JSAutoRealm realm(cx,closed);
    JS::RootedValue wrapped(cx,reason);
    if (!JS_WrapValue(cx,&wrapped) || !JS::RejectPromise(cx,closed,wrapped) ||
        !JS::SetAnyPromiseIsHandled(cx,closed)) return false;
  }
  return RejectReads(cx,stream,reason);
}
namespace {
bool Discard(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject promise(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
  if (!JS::ResolvePromise(cx,promise,JS::UndefinedHandleValue)) return false;
  args.rval().setObject(*promise);
  return true;
}
}
CallbackResult DefaultCancel(JSContext* cx,JS::HandleObject stream,JS::HandleValue reason,
                    JS::MutableHandleValue result,JS::HandleObject methodGlobal) {
  JS::RootedObject controller(cx,SlotObject(stream,SController)),promise(cx);
  if (IsByteStream(stream)) ResetByteQueue(controller);
  else ResetDefaultQueue(controller);
  auto outcome=DefaultCallbackPromise(cx,controller,DCancel,methodGlobal,reason,&promise);
  if (outcome!=CallbackResult::Success) return outcome;
  ClearDefaultAlgorithms(controller);
  promise=js::CheckedUnwrapStatic(promise);
  if (!promise) return CallbackOutcome(Error(cx,"ReadableStream cancel promise is unavailable"));
  {
    JSAutoRealm realm(cx,promise);
    // Donor cancellation adopts a separately fulfilled Promise, retaining its
    // observable reaction timing instead of returning undefined directly.
    JS::RootedObject discarded(cx,JS::NewPromiseObject(cx,nullptr));
    JS::RootedFunction callback(cx,js::NewFunctionWithReserved(cx,Discard,1,0,nullptr));
    if (!discarded || !callback) return CallbackResult::Failure;
    JS::RootedObject fulfilled(cx,JS_GetFunctionObject(callback)),rejected(cx);
    js::SetFunctionNativeReserved(fulfilled,0,JS::ObjectValue(*discarded));
    JS::RootedObject chained(cx,JS::CallOriginalPromiseThen(cx,promise,fulfilled,rejected));
    if (!chained) return CallbackResult::Failure;
    result.setObject(*chained);
  }
  return CallbackOutcome(JS_WrapValue(cx,result));
}
CallbackResult CancelReadable(JSContext* cx,JS::HandleObject stream,JS::HandleValue reason,
                     JS::MutableHandleValue result,JS::HandleObject methodGlobal) {
  DisturbReadable(stream);
  if (ReadableStatus(stream)!=StreamStatus::Readable) {
    {
      JSAutoRealm realm(cx,stream);
      JS::RootedValue stored(cx,JS::GetReservedSlot(stream,SError));
      if (ReadableStatus(stream)==StreamStatus::Closed) {
        if (!Settled(cx,JS::UndefinedHandleValue,result)) return CallbackResult::Failure;
      } else if (!Rejected(cx,stored,result)) return CallbackResult::Failure;
    }
    return CallbackOutcome(JS_WrapValue(cx,result));
  }
  if (!CloseReadable(cx,stream)) return CallbackResult::Failure;
  if (IsByteStream(stream) && !ByteCancelSteps(cx,stream)) return CallbackResult::Failure;
  return Source(stream)!=StreamSource::BufferedBody ? DefaultCancel(cx,stream,reason,result,methodGlobal) :
         CallbackOutcome(Settled(cx,JS::UndefinedHandleValue,result));
}
}
