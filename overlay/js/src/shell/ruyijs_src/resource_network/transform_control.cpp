// Transform coordinates the original stream states and readable output queue.
#include "resource_network/transform_internal.h"
#include "resource_network/state.h"
#include "js/Promise.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
void ClearTransformAlgorithms(JSObject* controller) {
  JS::SetReservedSlot(controller,CTransformer,JS::UndefinedValue());
  JS::SetReservedSlot(controller,CTransformCall,JS::UndefinedValue());
  JS::SetReservedSlot(controller,CFlushCall,JS::UndefinedValue());
}
bool SetTransformPressure(JSContext* cx,JS::HandleObject stream,bool pressure) {
  JSAutoRealm realm(cx,stream);
  JS::RootedObject replacement(cx,JS::NewPromiseObject(cx,nullptr)),old(cx,SlotObject(stream,TChange));
  if (!replacement || (old && !JS::ResolvePromise(cx,old,JS::UndefinedHandleValue))) return false;
  JS::SetReservedSlot(stream,TChange,JS::ObjectValue(*replacement));
  JS::SetReservedSlot(stream,TPressure,JS::BooleanValue(pressure));return true;
}
bool TransformErrorWritable(JSContext* cx,JS::HandleObject stream,JS::HandleValue reason) {
  JS::RootedObject controller(cx,SlotObject(stream,TController)),writable(cx,SlotObject(stream,TWritable));
  JS::RootedObject sink(cx,SlotObject(writable,WController));
  ClearTransformAlgorithms(controller);
  if (!WritableControllerError(cx,sink,reason)) return false;
  return !JS::GetReservedSlot(stream,TPressure).toBoolean() || SetTransformPressure(cx,stream,false);
}
bool TransformError(JSContext* cx,JS::HandleObject stream,JS::HandleValue reason) {
  JS::RootedObject readable(cx,SlotObject(stream,TReadable)),controller(cx,SlotObject(readable,SController));
  return DefaultError(cx,controller,reason) && TransformErrorWritable(cx,stream,reason);
}
bool TransformEnqueue(JSContext* cx,JS::HandleObject controller,JS::HandleValue chunk) {
  JS::RootedObject stream(cx,SlotObject(controller,CTransform)),readable(cx,SlotObject(stream,TReadable));
  JS::RootedObject source(cx,SlotObject(readable,SController));
  if (ReadableStatus(readable)==StreamStatus::Errored)
    return Error(cx,"TransformStreamDefaultController.enqueue: Cannot enqueue into a stream that has errored.");
  if (ReadableStatus(readable)==StreamStatus::Closed)
    return Error(cx,"TransformStreamDefaultController.enqueue: Cannot enqueue into a stream that is already closed.");
  if (DefaultFlags(source)&DCloseRequested)
    return Error(cx,"TransformStreamDefaultController.enqueue: Cannot enqueue into a stream that has been requested to be closed.");
  if (!DefaultEnqueue(cx,source,chunk)) {
    JS::RootedValue reason(cx);
    if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx,&reason)) return false;
    JS_ClearPendingException(cx);
    if (!TransformErrorWritable(cx,stream,reason)) return TransformNativeFailure(cx,stream);
    if (ReadableStatus(readable)!=StreamStatus::Errored) {
      JS_SetPendingException(cx,reason);return TransformNativeFailure(cx,stream);
    }
    reason=JS::GetReservedSlot(readable,SError);
    if (!JS_WrapValue(cx,&reason)) return false;
    JS_SetPendingException(cx,reason);return false;
  }
  const bool shouldPull=ReadableStatus(readable)==StreamStatus::Readable &&
      !(DefaultFlags(source)&DCloseRequested) && (DefaultFlags(source)&DStarted) &&
      (SlotObject(readable,SHead) || DefaultDesiredSize(source)>0);
  const bool pressure=!shouldPull;
  return pressure==JS::GetReservedSlot(stream,TPressure).toBoolean() ||
      SetTransformPressure(cx,stream,pressure) || TransformNativeFailure(cx,stream);
}
bool TransformTerminate(JSContext* cx,JS::HandleObject controller) {
  JS::RootedObject stream(cx,SlotObject(controller,CTransform)),readable(cx,SlotObject(stream,TReadable));
  JS::RootedObject source(cx,SlotObject(readable,SController));
  if (ReadableStatus(readable)==StreamStatus::Readable && !(DefaultFlags(source)&DCloseRequested) &&
      !DefaultClose(cx,source)) return TransformNativeFailure(cx,stream);
  Error(cx,"Terminating the stream");JS::RootedValue reason(cx);
  if (!JS_GetPendingException(cx,&reason)) return TransformNativeFailure(cx,stream);
  JS_ClearPendingException(cx);
  return TransformErrorWritable(cx,stream,reason) || TransformNativeFailure(cx,stream);
}
bool TransformNativeFailure(JSContext* cx,JS::HandleObject stream) {
  JS::RootedValue reason(cx);
  if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx,&reason)) return false;
  JS_ClearPendingException(cx);
  JS::RootedObject controller(cx,SlotObject(stream,TController));
  JS::RootedObject readable(cx,SlotObject(stream,TReadable)),source(cx,SlotObject(readable,SController));
  JS::RootedObject writable(cx,SlotObject(stream,TWritable)),sink(cx,SlotObject(writable,WController));
  // Each original owner gets a cleanup attempt even if the other one fails.
  // Retain the original failure; secondary cleanup errors never replace it.
  ClearTransformAlgorithms(controller);
  if (!DefaultError(cx,source,reason)) JS_ClearPendingException(cx);
  JS_SetPendingException(cx,reason);
  WritableNativeFailure(cx,sink);
  JS_ClearPendingException(cx);
  if (JS::GetReservedSlot(stream,TPressure).toBoolean()) {
    JSAutoRealm realm(cx,stream);
    JS::RootedObject change(cx,SlotObject(stream,TChange));
    JS::SetReservedSlot(stream,TPressure,JS::BooleanValue(false));
    if (change && !JS::ResolvePromise(cx,change,JS::UndefinedHandleValue)) JS_ClearPendingException(cx);
  }
  JS_SetPendingException(cx,reason);return false;
}
}
