// Default chunks and requests share the original stream's pending-read FIFO.
#include "resource_network/readable_controller.h"
#include "resource_network/readable_byte.h"
#include "resource_network/state.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Realm.h"
#include <algorithm>
#include <cmath>

namespace ruyijs::resource_network {
namespace {
bool CanEnqueue(JSObject* controller) {
  return !(DefaultFlags(controller)&DCloseRequested) &&
         ReadableStatus(SlotObject(controller,DStream))==StreamStatus::Readable;
}
bool FailWithPending(JSContext* cx,JS::HandleObject controller) {
  JS::RootedValue reason(cx);
  if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx,&reason)) return false;
  JS_ClearPendingException(cx);
  if (!DefaultError(cx,controller,reason)) return false;
  JS_SetPendingException(cx,reason);
  return false;
}
bool AppendChunk(JSContext* cx,JS::HandleObject controller,JS::HandleValue value,double size) {
  // Validation errors belong to the method realm; only private queue storage
  // enters the controller realm and wraps the chunk into that compartment.
  if (!std::isfinite(size) || size<0) return Error(cx,"Stream chunk size must be finite and nonnegative",true);
  JSAutoRealm realm(cx,controller);
  JS::RootedValue chunk(cx,value);
  if (!JS_WrapValue(cx,&chunk)) return false;
  JS::RootedObject entry(cx,JS_NewObjectWithGivenProto(cx,&kReadChunkClass,nullptr));
  if (!entry) return false;
  JS::SetReservedSlot(entry,ReadChunkValue,chunk);
  JS::SetReservedSlot(entry,ReadChunkSize,JS::DoubleValue(size));
  if (JSObject* tail=SlotObject(controller,DTail))
    JS::SetReservedSlot(tail,ReadChunkNext,JS::ObjectValue(*entry));
  else JS::SetReservedSlot(controller,DHead,JS::ObjectValue(*entry));
  JS::SetReservedSlot(controller,DTail,JS::ObjectValue(*entry));
  const double total=JS::GetReservedSlot(controller,DTotal).toNumber();
  JS::SetReservedSlot(controller,DTotal,JS::DoubleValue(total+size));
  return true;
}
}
bool DefaultClose(JSContext* cx,JS::HandleObject controller) {
  if (!CanEnqueue(controller)) return Error(cx,"ReadableStream controller cannot close");
  SetDefaultFlags(controller,DefaultFlags(controller)|DCloseRequested);
  if (SlotObject(controller,DHead)) return true;
  ClearDefaultAlgorithms(controller);
  JS::RootedObject stream(cx,SlotObject(controller,DStream));
  return CloseReadable(cx,stream);
}
bool DefaultEnqueue(JSContext* cx,JS::HandleObject controller,JS::HandleValue chunk) {
  if (!CanEnqueue(controller)) return Error(cx,"ReadableStream controller cannot enqueue");
  JS::RootedObject stream(cx,SlotObject(controller,DStream));
  if (SlotObject(stream,SHead)) {
    JS::RootedObject request(cx,PopReadRequest(stream));
    JS::RootedObject promise(cx,SlotObject(request,QTarget));
    if (!CompleteRead(cx,promise,false,chunk)) {
      if (!IsNativeReadCompletion(promise)) return false;
      FailNativeReadCompletion(cx,promise);return FailWithPending(cx,controller);
    }
  } else {
    double size=1;
    JS::RootedValue callback(cx,JS::GetReservedSlot(controller,DSize)),value(cx);
    if (!JS_WrapValue(cx,&callback)) return false;
    if (!callback.isUndefined() &&
        (!JS::Call(cx,JS::UndefinedHandleValue,callback,JS::HandleValueArray(chunk),&value) ||
         !JS::ToNumber(cx,value,&size))) return FailWithPending(cx,controller);
    // The donor appends after a reentrant size call without rechecking state or
    // serving reads created during that call. See reentrant-strategies WPT.
    if (!AppendChunk(cx,controller,chunk,size)) return FailWithPending(cx,controller);
  }
  return DefaultPullIfNeeded(cx,controller);
}
bool DefaultRead(JSContext* cx,JS::HandleObject stream,JS::HandleObject promise) {
  JS::RootedObject controller(cx,SlotObject(stream,SController));
  JS::RootedObject entry(cx,SlotObject(controller,DHead));
  if (!entry) return QueueRead(cx,stream,promise) && DefaultPullIfNeeded(cx,controller);
  JS::RootedValue chunk(cx,JS::GetReservedSlot(entry,ReadChunkValue));
  const double size=JS::GetReservedSlot(entry,ReadChunkSize).toNumber();
  const double total=JS::GetReservedSlot(controller,DTotal).toNumber();
  JS::SetReservedSlot(controller,DTotal,JS::DoubleValue(std::max(0.0,total-size)));
  auto next=JS::GetReservedSlot(entry,ReadChunkNext);
  JS::SetReservedSlot(controller,DHead,next);
  if (!next.isObject()) JS::SetReservedSlot(controller,DTail,JS::UndefinedValue());
  if ((DefaultFlags(controller)&DCloseRequested) && !next.isObject()) {
    ClearDefaultAlgorithms(controller);
    if (!CloseReadable(cx,stream)) return false;
  } else if (!DefaultPullIfNeeded(cx,controller)) return false;
  return JS_WrapValue(cx,&chunk) && CompleteRead(cx,promise,false,chunk);
}
bool DefaultError(JSContext* cx,JS::HandleObject controller,JS::HandleValue reason) {
  JS::RootedObject stream(cx,SlotObject(controller,DStream));
  if (IsByteStream(stream)) return ByteError(cx,controller,reason);
  if (ReadableStatus(stream)!=StreamStatus::Readable) return true;
  ResetDefaultQueue(controller);
  ClearDefaultAlgorithms(controller);
  return ErrorReadable(cx,stream,reason);
}
}
