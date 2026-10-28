// Original controller allocation and initialization after public conversion.
#include "resource_network/readable_controller.h"
#include "resource_network/readable_byte.h"
#include "resource_network/state.h"

namespace ruyijs::resource_network {
bool InitializeReadableController(State& state,JS::HandleObject stream,JS::HandleValue sourceValue,
    JS::HandleValue start,JS::HandleValue pull,JS::HandleValue cancel,JS::HandleValue size,
    double highWaterMark,bool bytes,uint64_t allocation) {
  JSContext* cx=state.cx;
  JS::RootedObject prototype(cx,bytes?state.byteController.prototype.get():state.defaultController.prototype.get());
  JS::RootedObject controller(cx);
  if (bytes) {
    JS::RootedObject requestPrototype(cx,state.byobRequest.prototype);
    if (!NewByteController(cx,stream,prototype,requestPrototype,allocation,highWaterMark,&controller)) return false;
    JS::SetReservedSlot(stream,SSource,JS::Int32Value(int(StreamSource::Byte)));
  } else {
    controller=JS_NewObjectWithGivenProto(cx,&kDefaultControllerClass,prototype);
    if (!controller) return false;
    ResetDefaultQueue(controller);
  }
  JS::SetReservedSlot(controller,DStream,JS::ObjectValue(*stream));
  JS::SetReservedSlot(controller,DSource,sourceValue);
  JS::SetReservedSlot(controller,DStart,start);
  JS::SetReservedSlot(controller,DPull,pull);
  JS::SetReservedSlot(controller,DCancel,cancel);
  JS::SetReservedSlot(controller,DSize,size);
  JS::SetReservedSlot(controller,DHwm,JS::DoubleValue(highWaterMark));
  SetDefaultFlags(controller,0);
  JS::SetReservedSlot(stream,SController,JS::ObjectValue(*controller));
  return StartDefaultReadable(cx,controller);
}
}
