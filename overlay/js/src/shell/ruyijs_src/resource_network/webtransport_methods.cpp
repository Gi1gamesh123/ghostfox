#include "resource_network/webtransport.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/experimental/TypedData.h"
#include <cstring>

namespace ruyijs::resource_network {
namespace {
bool Snapshot(JSContext* cx,const std::string& bytes,JS::MutableHandleValue result) {
  JS::RootedObject view(cx,JS_NewUint8Array(cx,bytes.size()));if(!view)return false;
  {JS::AutoCheckCannotGC nogc;bool shared=false;if(!bytes.empty())std::memcpy(JS_GetUint8ArrayData(view,&shared,nogc),bytes.data(),bytes.size());}
  result.setObject(*view);return true;
}
bool Close(JSContext* cx,State& s,JS::HandleObject transport,JS::HandleValue input) {
  if (!input.isNullOrUndefined() && !input.isObject()) return Error(cx,"WebTransport.close: Argument 1 can't be converted to a dictionary.");
  uint32_t code=0;std::string reason;JS::RootedValue value(cx);
  if (input.isObject()) {
    JS::RootedObject dictionary(cx,&input.toObject());
    if (!JS_GetProperty(cx,dictionary,"closeCode",&value) || (!value.isUndefined() && !JS::ToUint32(cx,value,&code))) return false;
    if (!JS_GetProperty(cx,dictionary,"reason",&value) || (!value.isUndefined() && !Convert(cx,value,&reason))) return false;
  }
  return TransportCloseSession(cx,transport,code,reason);
}
bool Group(JSContext* cx,State& s,JS::HandleObject transport,JS::MutableHandleValue result) {
  if (TransportStatus(transport)==TransportPhase::Closed || TransportStatus(transport)==TransportPhase::Failed)
    return TransportThrowDom(s,"InvalidStateError","WebTransport.createSendGroup: WebTransport closed or failed");
  JS::RootedObject group(cx,JS_NewObjectWithGivenProto(cx,&kTransportGroupClass,s.transportGroup.prototype)),none(cx);if (!group) return false;
  const double id=JS::GetReservedSlot(transport,WTNextGroup).toNumber();JS::SetReservedSlot(transport,WTNextGroup,JS::DoubleValue(id+1));
  JS::SetReservedSlot(group,0,JS::ObjectValue(*transport));JS::SetReservedSlot(group,1,JS::DoubleValue(id));
  WebTransportCommand command;command.action=WebTransportAction::Group;command.group=uint64_t(id);
  JS::RootedValue value(cx,JS::ObjectValue(*group));
  if (!TransportSubmit(cx,transport,command,none,none,JS::UndefinedHandleValue,value,JS::UndefinedHandleValue)) return false;
  result.setObject(*group);return true;
}
}
bool TransportMethodCall(JSContext* cx,unsigned argc,JS::Value* vp) {
  const bool okay=TransportGuard(cx,[&] {
    auto args=JS::CallArgsFromVp(argc,vp);const int kind=WebKit::Slot(args);
    const char* names[]={"getStats","exportKeyingMaterial","close","createBidirectionalStream","createUnidirectionalStream","createSendGroup"};
    JS::RootedObject transport(cx,args.thisv().isObject()?js::CheckedUnwrapStatic(&args.thisv().toObject()):nullptr);
    if (!transport || JS::GetClass(transport)!=&kTransportClass) {
      TransportBindingError(cx,names[kind],"WebTransport");return kind==WTClose || kind==WTCreateGroup?false:TransportRejectPending(cx,args.rval());
    }
    State* state=TransportState(cx,transport);if (!state) return false;
    JSAutoRealm realm(cx,transport);
    JS::RootedValue input(cx,args.get(0)),context(cx,args.get(1));
    if(!JS_WrapValue(cx,&input) || !JS_WrapValue(cx,&context))return false;
    if (kind==WTClose) {if (!Close(cx,*state,transport,input)) return false;args.rval().setUndefined();return true;}
    if (kind==WTCreateGroup) return Group(cx,*state,transport,args.rval());
    JS::RootedObject promise(cx,JS::NewPromiseObject(cx,nullptr)),none(cx);if (!promise) return false;
    if(kind==WTStats && TransportStatus(transport)==TransportPhase::Closed && !JS::GetReservedSlot(transport,WTClosing).toBoolean()) {
      if(!TransportClosedStats(cx,transport,promise))return false;
      args.rval().setObject(*promise);return true;
    }
    WebTransportCommand command;command.action=kind==WTStats?WebTransportAction::Stats:kind==WTExport?WebTransportAction::Export:WebTransportAction::OpenStream;
    JS::RootedValue data(cx),group(cx,JS::NullValue()),order(cx,JS::DoubleValue(0));
    if (kind==WTCreateBidi || kind==WTCreateUni) {
      int64_t priority;if (!TransportSendOptions(cx,transport,input,&group,&priority)) return TransportRejectPending(cx,args.rval());
      command.bidirectional=kind==WTCreateBidi;command.order=priority;order.setNumber(double(priority));data.setBoolean(command.bidirectional);
      if (group.isObject()) {command.hasGroup=true;command.group=uint64_t(JS::GetReservedSlot(&group.toObject(),1).toNumber());}
    }
    if (kind==WTExport) {
      if (!argc) {Error(cx,"WebTransport.exportKeyingMaterial: At least 1 argument required, but only 0 passed");return TransportRejectPending(cx,args.rval());}
      if (!TransportBytes(cx,input,&command.label) || (argc>1 && !TransportBytes(cx,context,&command.context))) return TransportRejectPending(cx,args.rval());
      if (command.label.size()>255 || command.context.size()>255) {
        Error(cx,command.label.size()>255?"WebTransport.exportKeyingMaterial: label must not exceed 255 bytes":"WebTransport.exportKeyingMaterial: context must not exceed 255 bytes",true);
        return TransportRejectPending(cx,args.rval());
      }
      JS::RootedObject pair(cx,JS::NewArrayObject(cx,2));if (!pair) return false;
      JS::RootedValue a(cx),b(cx);
      if (!Snapshot(cx,command.label,&a) || !Snapshot(cx,command.context,&b) || !JS_DefineElement(cx,pair,0,a,JSPROP_ENUMERATE) || !JS_DefineElement(cx,pair,1,b,JSPROP_ENUMERATE)) return false;
      data.setObject(*pair);
    }
    if (!TransportSubmit(cx,transport,command,promise,none,data,group,order)) return false;
    args.rval().setObject(*promise);return true;
  });
  auto args=JS::CallArgsFromVp(argc,vp);return okay && JS_WrapValue(cx,args.rval());
}
}
