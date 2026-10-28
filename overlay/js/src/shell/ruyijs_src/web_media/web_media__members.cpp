#include "runtime/web_kit.h"
#include "web_media/state.h"
#include "web_media/legacy_values.h"
#include "runtime/runtime.h"
#include "jsapi.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
namespace ruyijs::web_media {
namespace {
bool Call(State& s,JS::HandleObject self,const char* name,JS::HandleValue fn) {
  JSContext* cx=s.cx;
  JS::RootedString type(cx,JS_NewStringCopyZ(cx,"devicechange"));
  JS::RootedValue method(cx),receiver(cx,JS::ObjectValue(*self)),ignored(cx);
  JS::RootedValueArray<2> values(cx);
  if (!type || !JS_GetProperty(cx,self,name,&method)) return false;
  values[0].setString(type);values[1].set(fn);
  return JS::Call(cx,receiver,method,values,&ignored);
}
}
bool NavigatorMedia(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);auto& s=*static_cast<State*>(WebKit::Owner(args));
  if (!args.thisv().isObject() || &args.thisv().toObject()!=s.navigator)
    return WebKit::Fail(cx,"Navigator.mediaDevices called on an incompatible receiver");
  JS::RootedValue href(cx);
  if (!s.environment.URL(&href) || !href.isString()) return false;
  JS::RootedString text(cx,href.toString());std::string url;
  if (!ToUTF8(cx,text,&url)) return false;
  bool secure=url.rfind("https://",0)==0 || url=="about:blank";
  for (const char* host:{"http://localhost","http://127.0.0.1","http://[::1]"}) {
    std::string prefix(host);
    if (url.rfind(prefix,0)==0 && (url.size()==prefix.size() ||
        url[prefix.size()]==':' || url[prefix.size()]=='/')) secure=true;
  }
  if (secure) args.rval().setObject(*s.device);else args.rval().setUndefined();
  return true;
}
bool MediaMethod(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);auto& s=*static_cast<State*>(WebKit::Owner(args));
  int op=WebKit::Slot(args);JS::RootedObject self(cx);
  if (!WebKit::CheckBrand(cx,args,&kMediaClass,&self,"MediaDevices")) return false;
  if (!op) {
    // The profile's media.devices list (baseline: one audio input with no
    // label, id or group, Firefox's answer before anything is granted).
    const auto& devices=s.devices;
    JS::RootedObject array(cx,JS::NewArrayObject(cx,devices.size())),device(cx);
    if (!array) return false;
    for (size_t index=0;index<devices.size();++index) {
      if (!NewMediaDeviceInfo(s,devices[index].device_id.c_str(),devices[index].kind.c_str(),
                              devices[index].label.c_str(),devices[index].group_id.c_str(),
                              &device) ||
          !JS_DefineElement(cx,array,uint32_t(index),device,JSPROP_ENUMERATE)) return false;
    }
    JS::RootedValue value(cx,JS::ObjectValue(*array));return s.kit.Resolve(value,args.rval());
  }
  bool requested=false;
  if (args.get(0).isObject()) {
    JS::RootedObject constraints(cx,&args[0].toObject());
    for (const char* key:{"audio","video"}) {
      JS::RootedValue value(cx);
      if (!JS_GetProperty(cx,constraints,key,&value)) return false;
      requested |= JS::ToBoolean(value);
    }
  }
  JS::RootedValue constructor(cx),reason(cx);JS::RootedObject error(cx);
  if (!JS_GetProperty(cx,s.kit.page,requested?"DOMException":"TypeError",&constructor)) return false;
  JS::RootedString message(cx,JS_NewStringCopyZ(cx,requested?"No media devices are attached":"Audio or video must be requested"));
  JS::RootedString name(cx,JS_NewStringCopyZ(cx,"NotFoundError"));
  JS::RootedValueArray<2> values(cx);
  if (!message || !name) return false;
  values[0].setString(message);values[1].setString(name);
  if (!JS::Construct(cx,constructor,values,&error)) return false;
  reason.setObject(*error);return s.kit.RejectWith(reason,args.rval());
}
bool MediaHandler(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);auto& s=*static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx);
  if (!WebKit::CheckBrand(cx,args,&kMediaClass,&self,"MediaDevices")) return false;
  if (!argc) {args.rval().set(JS::GetReservedSlot(self,0));return true;}
  JS::RootedValue previous(cx,JS::GetReservedSlot(self,0)),next(cx,args.get(0));
  if (previous.isObject() && !Call(s,self,"removeEventListener",previous)) return false;
  if (!next.isObject() || !JS::IsCallable(&next.toObject())) next.setNull();
  if (next.isObject() && !Call(s,self,"addEventListener",next)) return false;
  JS::SetReservedSlot(self,0,next);args.rval().setUndefined();return true;
}
}
