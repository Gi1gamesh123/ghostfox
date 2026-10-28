#include "resource_network/webtransport.h"
#include "resource_network/writable_stream.h"
#include "js/Conversions.h"
#include "js/Promise.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
namespace {
JSObject* Endpoint(JS::HandleValue value,bool writable) {
  JSObject* stream=writable?WritableReceiver(value):StreamReceiver(value);
  if (!stream) return nullptr;
  JSObject* endpoint=SlotObject(stream,writable?WNativeOwner:SNativeOwner);
  return endpoint && JS::GetClass(endpoint)==&kTransportEndpointClass?endpoint:nullptr;
}
bool Stats(JSContext* cx,JS::HandleObject transport,JS::HandleObject endpoint,
    JS::HandleObject group,bool sending,JS::MutableHandleValue result) {
  JS::RootedObject promise(cx,JS::NewPromiseObject(cx,nullptr));if (!promise) return false;
  WebTransportCommand command;command.action=endpoint?WebTransportAction::StreamStats:WebTransportAction::Stats;
  if (endpoint)command.stream=uint64_t(JS::GetReservedSlot(endpoint,WEStream).toNumber());
  command.sending=sending;if (group){command.hasGroup=true;command.group=uint64_t(JS::GetReservedSlot(group,1).toNumber());}
  JS::RootedValue groupValue(cx,JS::ObjectOrNullValue(group)),data(cx,JS::BooleanValue(sending));
  if (!TransportSubmit(cx,transport,command,promise,endpoint,data,groupValue,JS::UndefinedHandleValue)) return false;
  result.setObject(*promise);return true;
}
}
bool TransportEndpointGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);const int kind=WebKit::Slot(args);
  JS::RootedObject endpoint(cx,Endpoint(args.thisv(),true));
  const bool expectedDatagram=kind>=2;
  if (!endpoint || (EndpointKind(JS::GetReservedSlot(endpoint,WEKind).toInt32())==EndpointKind::Datagram)!=expectedDatagram)
    return TransportBindingError(cx,kind%2?"get sendOrder":"get sendGroup",expectedDatagram?"WebTransportDatagramsWritable":"WebTransportSendStream");
  args.rval().set(JS::GetReservedSlot(endpoint,kind%2?WEOrder:WEGroup));return JS_WrapValue(cx,args.rval());
}
bool TransportEndpointSet(JSContext* cx,unsigned argc,JS::Value* vp) {
  return TransportGuard(cx,[&] {
    auto args=JS::CallArgsFromVp(argc,vp);const int kind=WebKit::Slot(args);JS::RootedObject endpoint(cx,Endpoint(args.thisv(),true));
    const bool expectedDatagram=kind>=2;
    if (!endpoint || (EndpointKind(JS::GetReservedSlot(endpoint,WEKind).toInt32())==EndpointKind::Datagram)!=expectedDatagram)
      return TransportBindingError(cx,kind%2?"set sendOrder":"set sendGroup",expectedDatagram?"WebTransportDatagramsWritable":"WebTransportSendStream");
    JS::RootedObject transport(cx,SlotObject(endpoint,WESession)),none(cx);JSAutoRealm realm(cx,transport);
    JS::RootedValue value(cx,args.get(0));if (!JS_WrapValue(cx,&value)) return false;
    WebTransportCommand command;command.stream=uint64_t(JS::GetReservedSlot(endpoint,WEStream).toNumber());
    if (kind%2) {int64_t order;if (!JS::ToInt64(cx,value,&order))return false;command.action=WebTransportAction::Order;command.order=order;value.setNumber(double(order));JS::SetReservedSlot(endpoint,WEOrder,value);}
    else {
      JSObject* group=value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr;
      if (!value.isNull() && (!group || JS::GetClass(group)!=&kTransportGroupClass || SlotObject(group,0)!=transport)) return Error(cx,"sendGroup belongs to a different WebTransport");
      command.action=WebTransportAction::SetGroup;command.hasGroup=!!group;if(group)command.group=uint64_t(JS::GetReservedSlot(group,1).toNumber());JS::SetReservedSlot(endpoint,WEGroup,value);
    }
    JS::RootedValue group(cx,JS::GetReservedSlot(endpoint,WEGroup)),order(cx,JS::GetReservedSlot(endpoint,WEOrder));
    if (!expectedDatagram && !TransportSubmit(cx,transport,command,none,endpoint,JS::UndefinedHandleValue,group,order)) return false;
    args.rval().setUndefined();return true;
  });
}
bool TransportEndpointStats(JSContext* cx,unsigned argc,JS::Value* vp) {
  const bool okay=TransportGuard(cx,[&] {
    auto args=JS::CallArgsFromVp(argc,vp);bool sending=WebKit::Slot(args)!=0;JS::RootedObject endpoint(cx,Endpoint(args.thisv(),sending)),none(cx);
    if (!endpoint) {TransportBindingError(cx,"getStats",sending?"WebTransportSendStream":"WebTransportReceiveStream");return TransportRejectPending(cx,args.rval());}
    JS::RootedObject transport(cx,SlotObject(endpoint,WESession));JSAutoRealm realm(cx,transport);
    return Stats(cx,transport,endpoint,none,sending,args.rval());
  });
  auto args=JS::CallArgsFromVp(argc,vp);return okay && JS_WrapValue(cx,args.rval());
}
bool TransportGroupStats(JSContext* cx,unsigned argc,JS::Value* vp) {
  const bool okay=TransportGuard(cx,[&] {
    auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject group(cx,args.thisv().isObject()?js::CheckedUnwrapStatic(&args.thisv().toObject()):nullptr),none(cx);
    if (!group || JS::GetClass(group)!=&kTransportGroupClass) {TransportBindingError(cx,"getStats","WebTransportSendGroup");return TransportRejectPending(cx,args.rval());}
    JS::RootedObject transport(cx,SlotObject(group,0));JSAutoRealm realm(cx,transport);return Stats(cx,transport,none,group,true,args.rval());
  });
  auto args=JS::CallArgsFromVp(argc,vp);return okay && JS_WrapValue(cx,args.rval());
}
}
