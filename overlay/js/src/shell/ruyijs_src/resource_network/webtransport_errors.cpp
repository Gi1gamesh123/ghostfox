#include "resource_network/webtransport.h"
#include "js/Conversions.h"
#include "js/ErrorReport.h"
#include "js/Exception.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include <cmath>

namespace ruyijs::resource_network {
const JSClass kTransportErrorDataClass={"WebTransport error data",JSCLASS_HAS_RESERVED_SLOTS(2)};
namespace {
const JSErrorFormatString* Format(void* data,unsigned) {return static_cast<JSErrorFormatString*>(data);}
bool Create(State& s,JS::HandleObject prototype,JS::HandleString message,bool session,
    JS::HandleValue code,JS::MutableHandleValue result) {
  JS::RootedObject data(s.cx,JS_NewObjectWithGivenProto(s.cx,&kTransportErrorDataClass,nullptr));if (!data) return false;
  JS::SetReservedSlot(data,0,JS::BooleanValue(session));JS::SetReservedSlot(data,1,code);
  JS::RootedValue payload(s.cx,JS::ObjectValue(*data));
  return s.transportExceptions.create(s.cx,prototype,message,"WebTransportError",payload,result);
}
}
bool TransportBindingError(JSContext* cx,const char* member,const char* iface) {
  JSErrorFormatString format={"RUYIJS_WEBTRANSPORT_BINDING","'{0}' called on an object that does not implement interface {1}.",2,JSEXN_TYPEERR};
  JS_ReportErrorNumberUTF8(cx,Format,&format,0,member,iface);return false;
}
bool TransportRejectPending(JSContext* cx,JS::MutableHandleValue result) {
  JS::RootedValue error(cx);if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx,&error)) return false;
  JS_ClearPendingException(cx);JS::RootedObject promise(cx,JS::CallOriginalPromiseReject(cx,error));
  if (!promise) return false;result.setObject(*promise);return true;
}
bool TransportDomError(State& s,const char* name,const char* message,JS::MutableHandleValue result) {
  JS::RootedString text(s.cx,JS_NewStringCopyZ(s.cx,message));
  JS::RootedObject prototype(s.cx,s.transportExceptionPrototype);
  return text && s.transportExceptions.create(s.cx,prototype,text,name,JS::UndefinedHandleValue,result);
}
bool TransportThrowDom(State& s,const char* name,const char* message) {
  JS::RootedValue value(s.cx);if (!TransportDomError(s,name,message,&value)) return false;
  JS_SetPendingException(s.cx,value);return false;
}
bool TransportErrorValue(State& s,const char* message,bool session,JS::HandleValue code,JS::MutableHandleValue result) {
  JS::RootedString text(s.cx,JS_NewStringCopyZ(s.cx,message));JS::RootedObject prototype(s.cx,s.transportError.prototype);
  return text && Create(s,prototype,text,session,code,result);
}
uint8_t TransportStreamErrorCode(State& s,JS::HandleValue reason) {
  JSObject* object=reason.isObject()?js::CheckedUnwrapStatic(&reason.toObject()):nullptr;
  JS::RootedValue payload(s.cx);
  if(!object || !s.transportExceptions.data(object,&payload) || !payload.isObject() ||
      JS::GetClass(&payload.toObject())!=&kTransportErrorDataClass)return 0;
  const auto code=JS::GetReservedSlot(&payload.toObject(),1);
  return code.isInt32()?uint8_t(code.toInt32()):0;
}
bool TransportErrorConstruct(JSContext* cx,unsigned argc,JS::Value* vp) {
  return TransportGuard(cx,[&] {
    auto args=JS::CallArgsFromVp(argc,vp);auto& s=*FunctionOwner(args);
    if (!args.isConstructing()) return Error(cx,"WebTransportError constructor: 'new' is required");
    JS::RootedObject prototype(cx);if (!TransportPrototype(cx,args,1,&prototype)) return false;
    if (!args.get(0).isNullOrUndefined() && !args[0].isObject()) return Error(cx,"WebTransportError constructor: Argument 1 can't be converted to a dictionary.");
    JS::RootedObject dictionary(cx,args.get(0).isObject()?&args[0].toObject():nullptr);
    JS::RootedString message(cx,JS_GetEmptyString(cx));JS::RootedValue value(cx),code(cx,JS::NullValue());
    if (dictionary) {
      if (!JS_GetProperty(cx,dictionary,"message",&value)) return false;
      if (!value.isUndefined() && !(message=JS::ToString(cx,value))) return false;
      if (!JS_GetProperty(cx,dictionary,"streamErrorCode",&value)) return false;
      if (!value.isUndefined()) {
        double number;if (!JS::ToNumber(cx,value,&number)) return false;
        number=std::isnan(number)?0:number<0?0:number>255?255:number;
        const double lower=std::floor(number),fraction=number-lower;
        number=fraction>.5 || (fraction==.5 && std::fmod(lower,2)!=0)?lower+1:lower;code.setInt32(int32_t(number));
      }
    }
    return Create(s,prototype,message,false,code,args.rval());
  });
}
bool TransportErrorGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);auto& s=*FunctionOwner(args);const int field=WebKit::Slot(args);
  JSObject* object=args.thisv().isObject()?js::CheckedUnwrapStatic(&args.thisv().toObject()):nullptr;
  JS::RootedValue value(cx);
  if (!object || !s.transportExceptions.data(object,&value) || !value.isObject() || JS::GetClass(&value.toObject())!=&kTransportErrorDataClass)
    return TransportBindingError(cx,field?"get streamErrorCode":"get source","WebTransportError");
  if (field) {args.rval().set(JS::GetReservedSlot(&value.toObject(),1));return true;}
  JS::RootedString text(cx,JS_NewStringCopyZ(cx,JS::GetReservedSlot(&value.toObject(),0).toBoolean()?"session":"stream"));
  if (!text) return false;args.rval().setString(text);return true;
}
}
