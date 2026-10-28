#include "resource_network/webtransport.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/experimental/TypedData.h"
#include "jsfriendapi.h"
#include <algorithm>

namespace ruyijs::resource_network {
namespace {
bool KeepOperation(JSContext*,unsigned argc,JS::Value* vp) {JS::CallArgsFromVp(argc,vp).rval().setUndefined();return true;}
bool RetainOperation(JSContext* cx,JS::HandleObject promise,JS::HandleObject transport) {
  if(!promise)return true;
  JS::RootedFunction function(cx,js::NewFunctionWithReserved(cx,KeepOperation,0,0,nullptr));if(!function)return false;
  JS::RootedObject reaction(cx,JS_GetFunctionObject(function));js::SetFunctionNativeReserved(reaction,0,JS::ObjectValue(*transport));
  return JS::AddPromiseReactionsIgnoringUnhandledRejection(cx,promise,reaction,reaction);
}
}
bool TransportResolve(JSContext* cx,JS::HandleObject promise,JS::HandleValue value) {
  return !promise || JS::ResolvePromise(cx,promise,value);
}
bool TransportReject(JSContext* cx,JS::HandleObject promise,JS::HandleValue reason) {
  return !promise || JS::RejectPromise(cx,promise,reason);
}
bool TransportSubmit(JSContext* cx,JS::HandleObject transport,const WebTransportCommand& command,
    JS::HandleObject promise,JS::HandleObject endpoint,JS::HandleValue data,JS::HandleValue group,JS::HandleValue order) {
  JS::RootedObject request(cx,JS_NewObjectWithGivenProto(cx,&kTransportRequestClass,nullptr));if (!request) return false;
  JS::SetReservedSlot(request,WRAction,JS::Int32Value(int(command.action)));
  JS::SetReservedSlot(request,WRPromise,JS::ObjectOrNullValue(promise));JS::SetReservedSlot(request,WREndpoint,JS::ObjectOrNullValue(endpoint));
  JS::SetReservedSlot(request,WRData,data);JS::SetReservedSlot(request,WRGroup,group);JS::SetReservedSlot(request,WROrder,order);
  JS::SetReservedSlot(request,WROffset,JS::DoubleValue(0));JS::SetReservedSlot(request,WRTicket,JS::DoubleValue(0));
  JS::SetReservedSlot(request,WRCode,JS::DoubleValue(command.code));JS::SetReservedSlot(request,WRSize,JS::DoubleValue(command.size));
  const bool closingStats=command.action==WebTransportAction::Stats && !command.hasGroup && JS::GetReservedSlot(transport,WTClosing).toBoolean();
  if (!closingStats && (TransportStatus(transport)==TransportPhase::Closed || TransportStatus(transport)==TransportPhase::Failed)) {
    State* state=TransportState(cx,transport);JS::RootedValue reason(cx);
    return state && TransportDomError(*state,"InvalidStateError","WebTransport closed or failed",&reason) && TransportReject(cx,promise,reason);
  }
  if(!RetainOperation(cx,promise,transport))return false;
  if (TransportStatus(transport)==TransportPhase::Connected && !TransportRetry(cx,transport,request))return false;
  JS::RootedObject last(cx,SlotObject(transport,WTRequests));
  if (!last)JS::SetReservedSlot(transport,WTRequests,JS::ObjectValue(*request));
  else {while(SlotObject(last,WRNext))last=SlotObject(last,WRNext);JS::SetReservedSlot(last,WRNext,JS::ObjectValue(*request));}
  TransportUpdateTaskRoot(transport);return true;
}
bool TransportRetry(JSContext* cx,JS::HandleObject transport,JS::HandleObject request) {
  WebTransportCommand command;command.action=WebTransportAction(JS::GetReservedSlot(request,WRAction).toInt32());
  command.code=uint32_t(JS::GetReservedSlot(request,WRCode).toNumber());command.size=uint32_t(JS::GetReservedSlot(request,WRSize).toNumber());
  JS::RootedObject endpoint(cx,SlotObject(request,WREndpoint));
  if (endpoint) command.stream=uint64_t(JS::GetReservedSlot(endpoint,WEStream).toNumber());
  JS::RootedValue group(cx,JS::GetReservedSlot(request,WRGroup)),order(cx,JS::GetReservedSlot(request,WROrder)),data(cx,JS::GetReservedSlot(request,WRData));
  if (group.isObject()) {command.hasGroup=true;command.group=uint64_t(JS::GetReservedSlot(&group.toObject(),1).toNumber());}
  if (order.isNumber()) command.order=int64_t(order.toNumber());
  if (command.action==WebTransportAction::OpenStream) command.bidirectional=data.toBoolean();
  else if (command.action==WebTransportAction::Write || command.action==WebTransportAction::Datagram) {
    JS::RootedObject view(cx,&data.toObject());
    const auto offset=size_t(JS::GetReservedSlot(request,WROffset).toNumber());
    const size_t remaining=JS_GetArrayBufferViewByteLength(view)-offset;
    const size_t size=command.action==WebTransportAction::Write?std::min<size_t>(remaining,65536):remaining;
    JS::AutoCheckCannotGC nogc;bool shared=false;
    const auto* bytes=static_cast<const char*>(JS_GetArrayBufferViewData(view,&shared,nogc));
    if(size)command.data.assign(bytes+offset,size);
  }
  else if (command.action==WebTransportAction::Export) {
    JS::RootedObject pair(cx,&data.toObject());JS::RootedValue value(cx);
    if (!JS_GetElement(cx,pair,0,&value) || !TransportBytes(cx,value,&command.label) ||
        !JS_GetElement(cx,pair,1,&value) || (!value.isUndefined() && !TransportBytes(cx,value,&command.context))) return false;
  }
  else if (command.action==WebTransportAction::StreamStats) command.sending=data.toBoolean();
  else if (command.action==WebTransportAction::Group && group.isObject()) command.group=uint64_t(JS::GetReservedSlot(&group.toObject(),1).toNumber());
  uint64_t ticket;std::string failure;
  if (!TransportSession(transport)->Submit(command,&ticket,&failure)) return Error(cx,failure.c_str());
  JS::SetReservedSlot(request,WRTicket,JS::DoubleValue(double(ticket)));return true;
}
}
