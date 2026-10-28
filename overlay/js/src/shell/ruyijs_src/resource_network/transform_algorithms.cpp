// The shared native algorithm records enter no author callback themselves.
#include "resource_network/transform_internal.h"
#include "resource_network/state.h"
#include "js/Promise.h"

namespace ruyijs::resource_network {
namespace {
CallbackResult Finish(JSContext* cx,JS::HandleObject stream,CallbackResult outcome) {
  if (outcome==CallbackResult::Failure) TransformNativeFailure(cx,stream);
  return outcome;
}
CallbackResult Resolved(JSContext* cx,JS::MutableHandleValue result) {
  JS::RootedObject promise(cx,JS::CallOriginalPromiseResolve(cx,JS::UndefinedHandleValue));
  if (!promise) return CallbackResult::Failure;
  result.setObject(*promise);return CallbackResult::Success;
}
CallbackResult Start(JSContext*,JS::HandleObject operation,JS::MutableHandleValue result) {
  result.set(JS::GetReservedSlot(operation,TNStart));return CallbackResult::Success;
}
CallbackResult SinkStart(JSContext* cx,JS::HandleObject stream,JS::HandleObject,
    JS::HandleValue,JS::MutableHandleValue result,CallbackContext) {return Start(cx,stream,result);}
CallbackResult SourceStart(JSContext* cx,JS::HandleObject stream,JS::HandleObject,JS::HandleObject,
    JS::HandleValue,JS::MutableHandleValue result) {return Start(cx,stream,result);}
CallbackResult Write(JSContext* cx,JS::HandleObject operation,JS::HandleObject,
    JS::HandleValue chunk,JS::MutableHandleValue result,CallbackContext context) {
  JS::RootedObject stream(cx,SlotObject(operation,TNStream));
  JS::RootedObject controller(cx,SlotObject(stream,TController));
  if (!JS::GetReservedSlot(stream,TPressure).toBoolean())
    return Finish(cx,stream,PerformTransform(cx,controller,chunk,result,context));
  JS::RootedObject promise(cx,SlotObject(stream,TChange));
  return Finish(cx,stream,CallbackOutcome(TransformThen(cx,controller,promise,TransformReaction::Pressure,chunk,result)));
}
CallbackResult Close(JSContext* cx,JS::HandleObject operation,JS::HandleObject,
    JS::HandleValue,JS::MutableHandleValue result,CallbackContext context) {
  JS::RootedObject stream(cx,SlotObject(operation,TNStream));
  JS::RootedObject controller(cx,SlotObject(stream,TController)),promise(cx);
  const auto outcome=TransformCallbackPromise(cx,controller,CFlushCall,JS::UndefinedHandleValue,&promise,context);
  if (outcome!=CallbackResult::Success) return Finish(cx,stream,outcome);
  ClearTransformAlgorithms(controller);
  return Finish(cx,stream,CallbackOutcome(TransformThen(cx,controller,promise,TransformReaction::Flush,JS::UndefinedHandleValue,result)));
}
CallbackResult Abort(JSContext* cx,JS::HandleObject operation,JS::HandleObject,
    JS::HandleValue reason,JS::MutableHandleValue result,CallbackContext) {
  JS::RootedObject stream(cx,SlotObject(operation,TNStream));
  if (!TransformError(cx,stream,reason)) return Finish(cx,stream,CallbackResult::Failure);
  return Finish(cx,stream,Resolved(cx,result));
}
CallbackResult Pull(JSContext* cx,JS::HandleObject operation,JS::HandleObject,JS::HandleObject,
    JS::HandleValue,JS::MutableHandleValue result) {
  JS::RootedObject stream(cx,SlotObject(operation,TNStream));
  if (!SetTransformPressure(cx,stream,false)) return Finish(cx,stream,CallbackResult::Failure);
  result.set(JS::GetReservedSlot(stream,TChange));return CallbackResult::Success;
}
CallbackResult Cancel(JSContext* cx,JS::HandleObject operation,JS::HandleObject,JS::HandleObject,
    JS::HandleValue reason,JS::MutableHandleValue result) {
  JS::RootedObject stream(cx,SlotObject(operation,TNStream));
  if (!TransformErrorWritable(cx,stream,reason)) return Finish(cx,stream,CallbackResult::Failure);
  return Finish(cx,stream,Resolved(cx,result));
}
}
const NativeSinkSteps kTransformSinkSteps={SinkStart,Write,Close,Abort};
const NativeSourceSteps kTransformSourceSteps={SourceStart,Pull,Cancel};
}
