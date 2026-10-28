// Original sink controller factory and start observation after public conversion.
#include "resource_network/writable_stream.h"
#include "resource_network/state.h"
#include "resource_network/abort.h"
#include "js/Promise.h"

namespace ruyijs::resource_network {
bool InitializeWritableController(State& state,JS::HandleObject stream,JS::HandleValue sinkValue,
    JS::HandleValue start,JS::HandleValue write,JS::HandleValue close,JS::HandleValue abort,
    JS::HandleValue size,double highWaterMark) {
  JSContext* cx=state.cx;
  JS::RootedObject prototype(cx,state.writableController.prototype);
  JS::RootedObject controller(cx,JS_NewObjectWithGivenProto(cx,&kSinkControllerClass,prototype)),signal(cx);
  if (!controller || !NewAbortSignal(state,&signal)) return false;
  JS::SetReservedSlot(controller,KStream,JS::ObjectValue(*stream));
  JS::SetReservedSlot(controller,KSink,sinkValue);
  JS::SetReservedSlot(controller,KStart,start);JS::SetReservedSlot(controller,KWrite,write);
  JS::SetReservedSlot(controller,KClose,close);JS::SetReservedSlot(controller,KAbort,abort);
  JS::SetReservedSlot(controller,KSize,size);JS::SetReservedSlot(controller,KHwm,JS::DoubleValue(highWaterMark));
  JS::SetReservedSlot(controller,KStarted,JS::BooleanValue(false));
  JS::SetReservedSlot(controller,KSignal,JS::ObjectValue(*signal));ResetWritableQueue(controller);
  JS::SetReservedSlot(stream,WController,JS::ObjectValue(*controller));
  JS::SetReservedSlot(stream,WPressure,JS::BooleanValue(highWaterMark<=0));
  JS::RootedValue value(cx);
  if (SinkCall(cx,controller,KStart,JS::UndefinedHandleValue,&value,
      {"WritableStream constructor: Refusing to execute function from global in which script is disabled.",&state})!=CallbackResult::Success) return false;
  JS::RootedObject promise(cx,JS::NewPromiseObject(cx,nullptr)),extra(cx);
  return promise && JS::SafeResolve(cx,promise,value) && ObserveSink(cx,controller,promise,SinkReaction::Start,extra);
}
}
