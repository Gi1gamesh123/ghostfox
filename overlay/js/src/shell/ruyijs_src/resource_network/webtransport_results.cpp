#include "resource_network/webtransport.h"
#include "js/ArrayBuffer.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/experimental/TypedData.h"
#include <cstring>

namespace ruyijs::resource_network {
bool TransportDeliverBytes(JSContext*,JS::HandleObject,const WebTransportEvent&);
namespace {
void Remove(JSObject* transport,JSObject* request,JSObject* previous) {
  const auto next=JS::GetReservedSlot(request,WRNext);
  JS::SetReservedSlot(previous?previous:transport,previous?WRNext:WTRequests,next);
  JS::SetReservedSlot(request,WRNext,JS::UndefinedValue());TransportUpdateTaskRoot(transport);
}
}
bool TransportResult(JSContext* cx,JS::HandleObject transport,const WebTransportEvent& event) {
  JS::RootedObject request(cx,SlotObject(transport,WTRequests)),previous(cx);
  while(request && JS::GetReservedSlot(request,WRTicket).toNumber()!=double(event.ticket)) {previous=request;request=SlotObject(request,WRNext);}
  if(!request)return true;
  JS::RootedObject promise(cx,SlotObject(request,WRPromise)),endpoint(cx,SlotObject(request,WREndpoint));
  State* state=TransportState(cx,transport);if(!state)return false;
  if(!event.success) {
    Remove(transport,request,previous);JS::RootedValue reason(cx);
    JS::RootedValue code(cx,event.clean?JS::Int32Value(event.code):JS::NullValue());
    if(!TransportErrorValue(*state,event.reason.c_str(),!endpoint,code,&reason))return false;
    return TransportReject(cx,promise,reason);
  }
  const auto action=WebTransportAction(JS::GetReservedSlot(request,WRAction).toInt32());
  if(action==WebTransportAction::Read && event.pending && event.data.empty() && !event.fin) {JS::SetReservedSlot(request,WRTicket,JS::DoubleValue(0));return true;}
  if(action==WebTransportAction::Write) {
    JS::RootedObject view(cx,SlotObject(request,WRData));const size_t length=JS_GetArrayBufferViewByteLength(view);
    const double consumed=JS::GetReservedSlot(request,WROffset).toNumber()+double(event.consumed);
    JS::SetReservedSlot(request,WROffset,JS::DoubleValue(consumed));
    if(consumed<length) {JS::SetReservedSlot(request,WRTicket,JS::DoubleValue(0));return true;}
  }
  Remove(transport,request,previous);JS::RootedValue result(cx);
  if(action==WebTransportAction::Read) {
    if(!TransportDeliverBytes(cx,endpoint,event))return false;
  } else if(action==WebTransportAction::OpenStream) {
    JS::RootedObject object(cx);JS::RootedValue group(cx,JS::GetReservedSlot(request,WRGroup));
    if(!TransportNewEndpoints(*state,transport,event.stream,JS::GetReservedSlot(request,WRData).toBoolean(),group,
        int64_t(JS::GetReservedSlot(request,WROrder).toNumber()),&object))return false;
    result.setObject(*object);
  } else if(action==WebTransportAction::Stats || action==WebTransportAction::StreamStats) {
    const bool group=JS::GetReservedSlot(request,WRGroup).isObject();
    if(action==WebTransportAction::Stats && !group)TransportRememberStats(transport,event);
    if(!TransportStatsObject(cx,event,action==WebTransportAction::StreamStats || group,
        JS::GetReservedSlot(request,WRData).isBoolean() && JS::GetReservedSlot(request,WRData).toBoolean(),&result))return false;
  } else if(action==WebTransportAction::Export) {
    JS::RootedObject view(cx,JS_NewUint8Array(cx,event.data.size()));if(!view)return false;
    {JS::AutoCheckCannotGC nogc;bool shared=false;if(!event.data.empty())std::memcpy(JS_GetUint8ArrayData(view,&shared,nogc),event.data.data(),event.data.size());}
    result.setObject(*view);
  }
  return TransportResolve(cx,promise,result);
}
}
