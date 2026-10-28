#include "resource_network/webtransport.h"
#include "resource_network/native_endpoints.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::resource_network {
namespace {
bool RejectDirection(JSContext* cx,JS::HandleObject transport,JS::HandleObject endpoint,
    bool sending,JS::HandleValue reason) {
  JS::RootedObject request(cx,SlotObject(transport,WTRequests)),previous(cx);
  while(request) {
    JS::RootedObject next(cx,SlotObject(request,WRNext));
    const auto action=WebTransportAction(JS::GetReservedSlot(request,WRAction).toInt32());
    const bool matches=sending?(action==WebTransportAction::Write || action==WebTransportAction::Finish):action==WebTransportAction::Read;
    if(SlotObject(request,WREndpoint)==endpoint && matches) {
      JS::SetReservedSlot(previous?previous.get():transport.get(),previous?WRNext:WTRequests,JS::ObjectOrNullValue(next));
      JS::SetReservedSlot(request,WRNext,JS::UndefinedValue());
      JS::RootedObject promise(cx,SlotObject(request,WRPromise));if(!TransportReject(cx,promise,reason))return false;
    } else previous=request;
    request=next;
  }
  TransportUpdateTaskRoot(transport);return true;
}
}
bool TransportStreamError(JSContext* cx,JS::HandleObject transport,const WebTransportEvent& event) {
  State* state=TransportState(cx,transport);if(!state)return false;
  const bool sending=event.kind==WebTransportEventKind::StopSending;
  JS::RootedValue code(cx,JS::Int32Value(event.code)),reason(cx);
  if(!TransportErrorValue(*state,sending?"WebTransportStream StopSending":"WebTransportStream Reset",false,code,&reason))return false;
  JS::RootedObject endpoints(cx,SlotObject(transport,WTEndpoints));uint32_t length=0;
  if(!JS::GetArrayLength(cx,endpoints,&length))return false;
  for(uint32_t i=0;i<length;++i) {
    JS::RootedValue value(cx);if(!JS_GetElement(cx,endpoints,i,&value))return false;if(!value.isObject())continue;
    JS::RootedObject endpoint(cx,&value.toObject());
    if(EndpointKind(JS::GetReservedSlot(endpoint,WEKind).toInt32())!=EndpointKind::ByteStream ||
        uint64_t(JS::GetReservedSlot(endpoint,WEStream).toNumber())!=event.stream)continue;
    JS::RootedObject controller(cx,SlotObject(endpoint,sending?WEWriteController:WEReadController));
    if(controller && !(sending?NativeWritableError(cx,controller,reason):NativeReadableError(cx,controller,reason)))return false;
    if(!RejectDirection(cx,transport,endpoint,sending,reason))return false;
    if(!sending) {
      JS::RootedObject pull(cx,SlotObject(endpoint,WEReadPending));
      JS::SetReservedSlot(endpoint,WEReadPending,JS::UndefinedValue());
      if(!TransportResolve(cx,pull,JS::UndefinedHandleValue))return false;
    }
  }
  return true;
}
}
