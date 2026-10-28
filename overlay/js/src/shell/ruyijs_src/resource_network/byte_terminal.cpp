// Byte terminal algorithms update the existing stream status and read FIFO.
#include "resource_network/readable_byte.h"
#include "resource_network/state.h"

namespace ruyijs::resource_network {
namespace {
void ClearDescriptors(JSObject* controller) {
  InvalidateByobRequest(controller);
  JS::SetReservedSlot(controller,BPendingHead,JS::UndefinedValue());
  JS::SetReservedSlot(controller,BPendingTail,JS::UndefinedValue());
}
}
bool ByteFailure(JSContext* cx,JS::HandleObject controller,bool rethrow) {
  JS::RootedValue reason(cx);
  const bool oom=JS_IsThrowingOutOfMemory(cx);
  if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx,&reason)) return false;
  JS_ClearPendingException(cx);
  if (!ByteError(cx,controller,reason)) return false;
  if (rethrow || oom) {JS_SetPendingException(cx,reason);return false;}
  return true;
}
bool ByteCommitFailure(JSContext* cx,JS::HandleObject controller) {
  JS::RootedObject stream(cx,SlotObject(controller,DStream));
  if (ReadableStatus(stream)!=StreamStatus::Closed) return ByteFailure(cx,controller);
  // EOF has already closed the stream, so reject outstanding read promises
  // directly before discarding descriptors. Never swallow an allocation failure.
  JS::RootedValue reason(cx);
  if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx,&reason)) return false;
  JS_ClearPendingException(cx);
  if (!RejectReads(cx,stream,reason)) return false;
  ClearDescriptors(controller);JS_SetPendingException(cx,reason);return false;
}
bool ByteError(JSContext* cx,JS::HandleObject controller,JS::HandleValue reason) {
  JS::RootedObject stream(cx,SlotObject(controller,DStream));
  if (ReadableStatus(stream)!=StreamStatus::Readable) return true;
  ClearDescriptors(controller);ResetByteQueue(controller);ClearDefaultAlgorithms(controller);
  return ErrorReadable(cx,stream,reason);
}
bool ByteClose(JSContext* cx,JS::HandleObject controller,ByteCloseKind kind,ByteCloseOutcome* outcome) {
  if (outcome) *outcome=ByteCloseOutcome::Failure;
  JS::RootedObject stream(cx,SlotObject(controller,DStream));
  if ((DefaultFlags(controller)&DCloseRequested) || ReadableStatus(stream)!=StreamStatus::Readable)
    return Error(cx,"Closing un-readable byte controller");
  if (ByteData(controller).total) {
    SetDefaultFlags(controller,DefaultFlags(controller)|DCloseRequested);
    if (outcome) *outcome=ByteCloseOutcome::Success;
    return true;
  }
  JSObject* first=SlotObject(controller,BPendingHead);
  if (first && PullData(first).filled%PullData(first).element) {
    Error(cx,(kind!=ByteCloseKind::Public || Source(stream)==StreamSource::BufferedBody)?"Leftover Bytes":"ReadableByteStreamController.close: Leftover Bytes");
    if (!outcome) return ByteFailure(cx,controller,kind!=ByteCloseKind::Buffered);
    // Classify only after this alignment error's original settlement succeeds;
    // a secondary allocation/settlement error remains Failure.
    const bool oom=JS_IsThrowingOutOfMemory(cx);JS::RootedValue reason(cx);
    if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx,&reason)) return false;
    JS_ClearPendingException(cx);
    if (!ByteError(cx,controller,reason)) return false;
    if (!oom) *outcome=ByteCloseOutcome::IncompleteElement;
    if (kind!=ByteCloseKind::Buffered || oom) {JS_SetPendingException(cx,reason);return false;}
    return true;
  }
  ClearDefaultAlgorithms(controller);
  const bool result=CloseReadable(cx,stream);
  if (outcome && result) *outcome=ByteCloseOutcome::Success;
  return result;
}
bool ByteCancelSteps(JSContext* cx,JS::HandleObject stream) {
  JS::RootedObject controller(cx,SlotObject(stream,SController));
  ClearDescriptors(controller);ResetByteQueue(controller);
  while (SlotObject(stream,SHead)) {
    JS::RootedObject request(cx,SlotObject(stream,SHead)),promise(cx,SlotObject(request,QTarget));
    const bool native=IsNativeReadCompletion(promise);
    if (native) PopReadRequest(stream);
    if (!CompleteRead(cx,promise,true,JS::UndefinedHandleValue,true)) {
      if (native) FailNativeReadCompletion(cx,promise);
      return ByteCommitFailure(cx,controller);
    }
    if (!native) PopReadRequest(stream);
  }
  return true;
}
void ByteRelease(JSObject* controller) {
  if (JSObject* first=SlotObject(controller,BPendingHead)) {
    PullData(first).kind=ReadKind::None;
    JS::SetReservedSlot(first,PNext,JS::UndefinedValue());
    JS::SetReservedSlot(controller,BPendingTail,JS::ObjectValue(*first));
  }
}
}
