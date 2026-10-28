// Pull-into setup and original default reader dispatch share the stream FIFO.
#include "resource_network/readable_byte.h"
#include "resource_network/state.h"
#include "js/ArrayBuffer.h"
#include "js/Promise.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
namespace {
bool RejectFailure(JSContext* cx,JS::HandleObject promise) {
  JS::RootedValue reason(cx);
  if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx,&reason)) return false;
  JS_ClearPendingException(cx);
  return RejectReadCompletion(cx,promise,reason);
}
bool QueuePullIntoRead(JSContext* cx,JS::HandleObject stream,JS::HandleObject promise,JS::HandleObject descriptor) {
  JSAutoRealm realm(cx,stream);
  JS::RootedObject request(cx),controller(cx,SlotObject(stream,SController));
  // Publish both FIFO edges only after every allocating preparation succeeds.
  if (!PrepareReadRequest(cx,stream,promise,&request) || !ScheduleRead(cx,stream)) return false;
  AppendPullInto(controller,descriptor);AppendReadRequest(stream,request);return true;
}
}
bool ByteRead(JSContext* cx,JS::HandleObject stream,JS::HandleObject promise,
               JS::HandleObject input,const ByteView& view,uint64_t minimum) {
  JS::RootedObject controller(cx,SlotObject(stream,SController)),buffer(cx),descriptor(cx);
  if (!TransferByteBuffer(cx,input,&buffer)) return RejectFailure(cx,promise);
  PullInto data{view.bufferLength,view.offset,view.length,0,minimum*view.element,view.element,view.type,ReadKind::Byob};
  if (!NewPullInto(cx,controller,buffer,data,&descriptor)) return false;
  if (SlotObject(controller,BPendingHead)) {
    return QueuePullIntoRead(cx,stream,promise,descriptor);
  }
  if (ReadableStatus(stream)==StreamStatus::Closed) {
    JS::RootedObject empty(cx);
    if (!NewByteView(cx,view.type,buffer,view.offset,0,&empty)) return false;
    JS::RootedValue value(cx,JS::ObjectValue(*empty));return CompleteRead(cx,promise,true,value,true);
  }
  if (ByteData(controller).total) {
    bool ready=false;
    if (!FillPullInto(cx,controller,descriptor,&ready)) return false;
    if (ready) {
      JS::RootedObject filled(cx);
      if (!PullIntoView(cx,descriptor,&filled) || !ByteQueueDrained(cx,controller)) return false;
      JS::RootedValue value(cx,JS::ObjectValue(*filled));return CompleteRead(cx,promise,false,value,true);
    }
    if (DefaultFlags(controller)&DCloseRequested) {
      Error(cx,"Close Requested True during Pull Into");
      JS::RootedValue reason(cx);
      if (!JS_GetPendingException(cx,&reason)) return false;
      JS_ClearPendingException(cx);
      return ByteError(cx,controller,reason) && RejectReadCompletion(cx,promise,reason);
    }
  }
  return QueuePullIntoRead(cx,stream,promise,descriptor) && DefaultPullIfNeeded(cx,controller);
}
bool ByteDefaultRead(JSContext* cx,JS::HandleObject stream,JS::HandleObject promise) {
  JS::RootedObject controller(cx,SlotObject(stream,SController));
  if (ByteData(controller).total) {
    JS::RootedObject entry(cx,SlotObject(controller,DHead)),buffer(cx,SlotObject(entry,BQBuffer)),view(cx);
    const auto data=RegionData(entry);
    auto next=JS::GetReservedSlot(entry,BQNext);JS::SetReservedSlot(controller,DHead,next);
    if (!next.isObject()) JS::SetReservedSlot(controller,DTail,JS::UndefinedValue());
    ByteData(controller).total-=data.length;
    if (!ByteQueueDrained(cx,controller) || !NewByteView(cx,JS::Scalar::Uint8,buffer,data.offset,data.length,&view)) return false;
    JS::RootedValue value(cx,JS::ObjectValue(*view));return CompleteRead(cx,promise,false,value);
  }
  const uint64_t allocation=ByteData(controller).autoAllocate;
  if (allocation) {
    JS::RootedObject buffer(cx,JS::NewArrayBuffer(cx,size_t(allocation))),descriptor(cx);
    if (!buffer) return RejectFailure(cx,promise);
    PullInto data{allocation,0,allocation,0,1,1,JS::Scalar::Uint8,ReadKind::Default};
    if (!NewPullInto(cx,controller,buffer,data,&descriptor)) return false;
    return QueuePullIntoRead(cx,stream,promise,descriptor) && DefaultPullIfNeeded(cx,controller);
  }
  return QueueRead(cx,stream,promise) && DefaultPullIfNeeded(cx,controller);
}
}
