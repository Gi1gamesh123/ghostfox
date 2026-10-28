// Erroring waits for the one in-flight operation before terminal settlement.
#include "resource_network/writable_stream.h"
#include "resource_network/state.h"
#include "js/Realm.h"
#include <initializer_list>

namespace ruyijs::resource_network {
bool WritableRejectClosed(JSContext* cx,JS::HandleObject stream) {
  JS::RootedValue reason(cx,JS::GetReservedSlot(stream,WError));
  JS::RootedObject close(cx,SlotObject(stream,WClose)),writer(cx,SlotObject(stream,WWriter));
  if (close) {
    if (!WritablePromise(cx,close,true,reason)) return false;
    JS::SetReservedSlot(stream,WClose,JS::UndefinedValue());
  }
  return !writer || WriterReject(cx,writer,VClosed,reason);
}
CallbackResult WritableStartErroring(JSContext* cx,JS::HandleObject stream,JS::HandleValue reason,CallbackContext context) {
  if (!WritableStore(cx,stream,WError,reason)) return CallbackResult::Failure;
  SetWritableStatus(stream,WriteStatus::Erroring);
  JS::RootedObject writer(cx,SlotObject(stream,WWriter)),controller(cx,SlotObject(stream,WController));
  if (writer && !WriterReject(cx,writer,VReady,reason)) return CallbackOutcome(WritableNativeFailure(cx,controller));
  if (WritableInFlight(stream) || !JS::GetReservedSlot(controller,KStarted).toBoolean()) return CallbackResult::Success;
  return WritableFinishErroring(cx,stream,context);
}
CallbackResult WritableFinishErroring(JSContext* cx,JS::HandleObject stream,CallbackContext context) {
  JSAutoRealm realm(cx,stream);
  SetWritableStatus(stream,WriteStatus::Errored);
  JS::RootedObject controller(cx,SlotObject(stream,WController)),request(cx),promise(cx);
  ResetWritableQueue(controller);
  JS::RootedValue error(cx,JS::GetReservedSlot(stream,WError));
  while ((request=SlotObject(stream,WHead))) {
    promise=SlotObject(request,FPromise);
    if (!WritablePromise(cx,promise,true,error)) return CallbackResult::Failure;
    PopWriteRequest(stream);
  }
  JS::RootedObject abort(cx,SlotObject(stream,WAbort));
  if (!abort) return CallbackOutcome(WritableRejectClosed(cx,stream));
  const bool wasErroring=JS::GetReservedSlot(stream,WAbortErroring).toBoolean();
  JS::RootedValue reason(cx,JS::GetReservedSlot(stream,WAbortReason));
  if (wasErroring) {
    if (!WritablePromise(cx,abort,true,error)) return CallbackResult::Failure;
  } else {
    const auto result=SinkPromise(cx,controller,KAbort,reason,&promise,context);
    if (result!=CallbackResult::Success) return result;
    ClearSinkAlgorithms(controller);
    if (!ObserveSink(cx,controller,promise,SinkReaction::Abort,abort)) return CallbackResult::Failure;
  }
  // Keep the request traced until either settled or observed successfully.
  JS::SetReservedSlot(stream,WAbort,JS::UndefinedValue());
  JS::SetReservedSlot(stream,WAbortReason,JS::UndefinedValue());
  return CallbackOutcome(!wasErroring || WritableRejectClosed(cx,stream));
}
CallbackResult WritableDealRejection(JSContext* cx,JS::HandleObject stream,JS::HandleValue reason,CallbackContext context) {
  return WritableStatus(stream)==WriteStatus::Writable?WritableStartErroring(cx,stream,reason,context):WritableFinishErroring(cx,stream,context);
}
bool WritableControllerError(JSContext* cx,JS::HandleObject controller,JS::HandleValue reason) {
  JS::RootedObject stream(cx,SlotObject(controller,KStream));
  if (WritableStatus(stream)!=WriteStatus::Writable) return true;
  ClearSinkAlgorithms(controller);return WritableStartErroring(cx,stream,reason)==CallbackResult::Success;
}
bool WritableNativeFailure(JSContext* cx,JS::HandleObject controller) {
  JS::RootedValue error(cx);
  if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx,&error)) return false;
  JS_ClearPendingException(cx);
  JS::RootedObject stream(cx,SlotObject(controller,KStream)),promise(cx),request(cx);
  if (!WritableStore(cx,stream,WError,error)) return false;
  SetWritableStatus(stream,WriteStatus::Errored);ClearSinkAlgorithms(controller);ResetWritableQueue(controller);
  for (unsigned slot:{WFlightWrite,WFlightClose,WAbort}) {
    promise=SlotObject(stream,slot);
    if (promise && !WritablePromise(cx,promise,true,error)) return false;
    JS::SetReservedSlot(stream,slot,JS::UndefinedValue());
  }
  JS::SetReservedSlot(stream,WAbortReason,JS::UndefinedValue());
  while ((request=SlotObject(stream,WHead))) {
    promise=SlotObject(request,FPromise);
    if (!WritablePromise(cx,promise,true,error)) return false;
    PopWriteRequest(stream);
  }
  JS::RootedObject writer(cx,SlotObject(stream,WWriter));
  if ((writer && !WriterReject(cx,writer,VReady,error)) || !WritableRejectClosed(cx,stream)) return false;
  JS_SetPendingException(cx,error);return false;
}
}
