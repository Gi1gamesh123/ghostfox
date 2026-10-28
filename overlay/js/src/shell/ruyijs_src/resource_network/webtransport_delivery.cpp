#include "resource_network/webtransport.h"
#include "resource_network/native_endpoints.h"
#include "resource_network/readable_byte.h"
#include "js/Array.h"
#include "js/ArrayBuffer.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/experimental/TypedData.h"
#include <cstring>

namespace ruyijs::resource_network {
namespace {
bool SettlePull(JSContext* cx,JS::HandleObject endpoint) {
  JS::RootedObject promise(cx,SlotObject(endpoint,WEReadPending));JS::SetReservedSlot(endpoint,WEReadPending,JS::UndefinedValue());
  return TransportResolve(cx,promise,JS::UndefinedHandleValue);
}
bool EnqueueBytes(JSContext* cx,JS::HandleObject controller,const std::string& bytes) {
  if(bytes.empty() && JS::GetClass(controller)==&kByteControllerClass)return true;
  JS::RootedObject view(cx,JS_NewUint8Array(cx,bytes.size()));if(!view)return false;
  {JS::AutoCheckCannotGC nogc;bool shared=false;if(!bytes.empty())std::memcpy(JS_GetUint8ArrayData(view,&shared,nogc),bytes.data(),bytes.size());}
  JS::RootedValue value(cx,JS::ObjectValue(*view));return NativeReadableEnqueue(cx,controller,value);
}
}
bool TransportDeliver(JSContext* cx,JS::HandleObject transport,const WebTransportEvent& event) {
  State* state=TransportState(cx,transport);if(!state)return false;
  switch(event.kind) {
    case WebTransportEventKind::Ready: {
      if(TransportStatus(transport)!=TransportPhase::Connecting)return true;
      SetTransportStatus(transport,TransportPhase::Connected);JS::SetReservedSlot(transport,WTWasReady,JS::BooleanValue(true));
      JS::RootedValue protocol(cx);if(!Value(cx,event.protocol,&protocol))return false;JS::SetReservedSlot(transport,WTProtocol,protocol);
      JS::RootedObject datagrams(cx,SlotObject(transport,WTDatagrams)),ready(cx,SlotObject(transport,WTReady));
      JS::SetReservedSlot(datagrams,WDMaxSize,JS::DoubleValue(event.maxDatagramSize));return TransportResolve(cx,ready,JS::UndefinedHandleValue);
    }
    case WebTransportEventKind::Result:return TransportResult(cx,transport,event);
    case WebTransportEventKind::Closed:return TransportClosed(cx,transport,event);
    case WebTransportEventKind::Error:return TransportFail(cx,transport,event.reason.c_str());
    case WebTransportEventKind::StreamReset:
    case WebTransportEventKind::StopSending:return TransportStreamError(cx,transport,event);
    case WebTransportEventKind::Draining: {
      JS::RootedObject promise(cx,SlotObject(transport,WTDraining));return TransportResolve(cx,promise,JS::UndefinedHandleValue);
    }
    case WebTransportEventKind::Stream: {
      JS::RootedObject result(cx),controller(cx,SlotObject(transport,event.bidirectional?WTBidiController:WTUniController));
      if(ReadableStatus(SlotObject(controller,DStream))!=StreamStatus::Readable)return true;
      if(!TransportNewEndpoints(*state,transport,event.stream,event.bidirectional,JS::NullHandleValue,0,&result))return false;
      JS::RootedValue value(cx,JS::ObjectValue(*result));if(!NativeReadableEnqueue(cx,controller,value))return false;
      JS::RootedObject stream(cx,SlotObject(controller,DStream)),endpoint(cx,SlotObject(stream,SNativeOwner));return SettlePull(cx,endpoint);
    }
    case WebTransportEventKind::Datagram: {
      JS::RootedObject datagrams(cx,SlotObject(transport,WTDatagrams)),stream(cx,SlotObject(datagrams,WDReadable));
      JS::RootedObject endpoint(cx,SlotObject(stream,SNativeOwner)),controller(cx,SlotObject(endpoint,WEReadController));
      if(ReadableStatus(stream)!=StreamStatus::Readable)return true;
      // Donor datagrams use the original default controller, preserving empty packets.
      return EnqueueBytes(cx,controller,event.data) && SettlePull(cx,endpoint);
    }
  }
  return true;
}
bool TransportDeliverBytes(JSContext* cx,JS::HandleObject endpoint,const WebTransportEvent& event) {
  JS::RootedObject controller(cx,SlotObject(endpoint,WEReadController)),stream(cx,SlotObject(endpoint,WEReadable));
  if(ReadableStatus(stream)!=StreamStatus::Readable)return true;
  if(!EnqueueBytes(cx,controller,event.data))return false;
  if(event.fin && !NativeReadableClose(cx,controller))return false;
  return SettlePull(cx,endpoint);
}
}
