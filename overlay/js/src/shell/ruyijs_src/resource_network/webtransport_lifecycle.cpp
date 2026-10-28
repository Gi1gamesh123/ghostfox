#include "resource_network/webtransport.h"
#include "resource_network/native_endpoints.h"
#include "resource_network/readable_controller.h"
#include "resource_network/writable_stream.h"
#include "js/Array.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "jsfriendapi.h"

namespace ruyijs::resource_network {
namespace {
bool RejectRequests(JSContext* cx,JS::HandleObject transport,JS::HandleValue reason,bool deferStats=false) {
  State* state=TransportState(cx,transport);if(!state)return false;
  JS::RootedObject request(cx,SlotObject(transport,WTRequests));JS::SetReservedSlot(transport,WTRequests,JS::UndefinedValue());
  JS::RootedObject retained(cx);
  while(request) {
    JS::RootedObject promise(cx,SlotObject(request,WRPromise));JS::RootedValue specific(cx,reason);
    const auto action=WebTransportAction(JS::GetReservedSlot(request,WRAction).toInt32());
    if(action==WebTransportAction::Stats && !JS::GetReservedSlot(request,WRGroup).isObject() && TransportStatus(transport)==TransportPhase::Closed) {
      JS::RootedObject next(cx,SlotObject(request,WRNext));
      if(deferStats) {
        JS::SetReservedSlot(request,WRTicket,JS::DoubleValue(0));JS::SetReservedSlot(request,WRNext,JS::UndefinedValue());
        JS::SetReservedSlot(retained?retained.get():transport.get(),retained?WRNext:WTRequests,JS::ObjectValue(*request));retained=request;
      } else if(!TransportClosedStats(cx,transport,promise))return false;
      request=next;continue;
    }
    if (action==WebTransportAction::OpenStream) {
      const bool bidi=JS::GetReservedSlot(request,WRData).toBoolean();
      if (!TransportDomError(*state,"InvalidStateError",bidi?"Transport close/errored before CreateBidirectional started":"Transport close/errored during CreateUnidirectional",&specific))return false;
    } else if (action==WebTransportAction::Stats && !TransportDomError(*state,"InvalidStateError","WebTransport failed",&specific))return false;
    if (!TransportReject(cx,promise,specific))return false;
    request=SlotObject(request,WRNext);
  }
  return true;
}
bool FinishEndpoints(JSContext* cx,JS::HandleObject transport,JS::HandleValue error,bool clean) {
  JS::RootedObject endpoints(cx,SlotObject(transport,WTEndpoints));uint32_t length=0;if(!JS::GetArrayLength(cx,endpoints,&length))return false;
  for(uint32_t i=0;i<length;++i) {
    JS::RootedValue value(cx);if(!JS_GetElement(cx,endpoints,i,&value))return false;if(!value.isObject())continue;
    JS::RootedObject endpoint(cx,&value.toObject()),read(cx,SlotObject(endpoint,WEReadController)),write(cx,SlotObject(endpoint,WEWriteController));
    const auto kind=EndpointKind(JS::GetReservedSlot(endpoint,WEKind).toInt32());
    if (read) {
      JS::RootedObject stream(cx,SlotObject(read,DStream));
      if (ReadableStatus(stream)==StreamStatus::Readable && !(DefaultFlags(read)&DCloseRequested)) {
        if (clean && (kind==EndpointKind::IncomingBidi || kind==EndpointKind::IncomingUni)) {if(!NativeReadableClose(cx,read))return false;}
        else if (!NativeReadableError(cx,read,error))return false;
      }
    }
    if(write && !NativeWritableError(cx,write,error))return false;
    JS::RootedObject pull(cx,SlotObject(endpoint,WEReadPending));if(!TransportResolve(cx,pull,JS::UndefinedHandleValue))return false;
    JS::SetReservedSlot(endpoint,WEReadPending,JS::UndefinedValue());
  }
  return true;
}
}
bool TransportProcess(JSContext* cx,JS::HandleObject transport) {
  std::vector<WebTransportEvent> events;std::string failure;
  if (!TransportSession(transport)->Poll(&events,&failure))return TransportFail(cx,transport,failure.c_str());
  for(const auto& event:events)if(!TransportDeliver(cx,transport,event))return false;
  if(TransportStatus(transport)!=TransportPhase::Connected)return true;
  JS::RootedObject request(cx,SlotObject(transport,WTRequests));
  while(request) {if(JS::GetReservedSlot(request,WRTicket).toNumber()==0 && !TransportRetry(cx,transport,request))return false;request=SlotObject(request,WRNext);}
  return true;
}
bool TransportFail(JSContext* cx,JS::HandleObject transport,const char* message) {
  if(TransportStatus(transport)==TransportPhase::Closed && JS::GetReservedSlot(transport,WTClosing).toBoolean()) {
    WebTransportEvent closed;closed.clean=true;return TransportClosed(cx,transport,closed);
  }
  if (TransportStatus(transport)==TransportPhase::Failed || TransportStatus(transport)==TransportPhase::Closed)return true;
  State* state=TransportState(cx,transport);if(!state)return false;
  SetTransportStatus(transport,TransportPhase::Failed);
  TransportUpdateTaskRoot(transport);
  if (auto* session=TransportSession(transport))session->Stop();
  JS::RootedValue error(cx);if(!TransportErrorValue(*state,message,true,JS::NullHandleValue,&error))return false;
  JS::RootedObject ready(cx,SlotObject(transport,WTReady)),closed(cx,SlotObject(transport,WTClosed));
  return TransportReject(cx,closed,error) && TransportReject(cx,ready,error) &&
      RejectRequests(cx,transport,error) && FinishEndpoints(cx,transport,error,false);
}
bool TransportCloseSession(JSContext* cx,JS::HandleObject transport,uint32_t code,const std::string& reason) {
  const auto phase=TransportStatus(transport);
  if(phase==TransportPhase::Closed || phase==TransportPhase::Failed)return true;
  if(phase==TransportPhase::Connecting)return TransportFail(cx,transport,"close() called on WebTransport while connecting");
  WebTransportCommand command;command.action=WebTransportAction::Close;command.code=code;command.reason=reason;
  if(command.reason.size()>1024) {size_t end=1024;while(end && (static_cast<unsigned char>(command.reason[end])&0xc0)==0x80)--end;command.reason.resize(end);}
  uint64_t ticket;std::string failure;
  if(!TransportSession(transport)->Submit(command,&ticket,&failure))return TransportFail(cx,transport,failure.c_str());
  JS::SetReservedSlot(transport,WTClosing,JS::BooleanValue(true));
  TransportUpdateTaskRoot(transport);
  WebTransportEvent closed;closed.clean=true;closed.pending=true;closed.code=code;closed.reason=reason;
  return TransportClosed(cx,transport,closed);
}
bool TransportClosed(JSContext* cx,JS::HandleObject transport,const WebTransportEvent& event) {
  if(!event.clean)return TransportFail(cx,transport,event.reason.c_str());
  State* state=TransportState(cx,transport);if(!state)return false;SetTransportStatus(transport,TransportPhase::Closed);
  TransportRememberStats(transport,event);
  if(!event.pending) {
    JS::SetReservedSlot(transport,WTClosing,JS::BooleanValue(false));
    if(auto* session=TransportSession(transport))session->Stop();
  }
  JS::RootedObject info(cx,JS_NewPlainObject(cx)),closed(cx,SlotObject(transport,WTClosed));JS::RootedValue reason(cx),code(cx,JS::DoubleValue(event.code)),error(cx);
  if(!info || !Value(cx,event.reason,&reason) || !JS_DefineProperty(cx,info,"closeCode",code,JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx,info,"reason",reason,JSPROP_ENUMERATE) || !TransportErrorValue(*state,"WebTransport closed",true,JS::NullHandleValue,&error))return false;
  JS::RootedValue value(cx,JS::ObjectValue(*info));
  const bool okay=TransportResolve(cx,closed,value) && RejectRequests(cx,transport,error,event.pending) && FinishEndpoints(cx,transport,error,true);
  TransportUpdateTaskRoot(transport);return okay;
}
}
