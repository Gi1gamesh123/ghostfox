#include "web_canvas/internal.h"
#include "interaction/realm.h"
#include "js/CallAndConstruct.h"

namespace ruyijs::web_canvas {
namespace {
bool Delegate(State& s,JS::HandleObject self,const char* method,const char* type,
              JS::HandleValue callback) {
  JS::RootedObject proto(s.cx,s.events.prototype());
  JS::RootedValue fn(s.cx),ignored(s.cx);
  JS::RootedString name(s.cx,JS_NewStringCopyZ(s.cx,type));
  if (!name || !JS_GetProperty(s.cx,proto,method,&fn)) return false;
  JS::RootedValueArray<2> args(s.cx);args[0].setString(name);args[1].set(callback);
  return JS::Call(s.cx,self,fn,args,&ignored);
}
bool HandlerGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  int slot=WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!WebKit::CheckBrand(cx,args,&kOffscreenClass,&self,"OffscreenCanvas")) return false;
  JS::Value value=JS::GetReservedSlot(self,slot);
  args.rval().set(value.isUndefined()?JS::NullValue():value);return true;
}
bool HandlerSet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  State& s=*detail::Self(args);
  int slot=WebKit::Slot(args);
  const char* type=slot==3?"contextlost":"contextrestored";
  JS::RootedObject self(cx);
  if (!WebKit::CheckBrand(cx,args,&kOffscreenClass,&self,"OffscreenCanvas")) return false;
  JS::RootedValue old(cx,JS::GetReservedSlot(self,slot)),value(cx,args.get(0));
  if (old.isObject() && !Delegate(s,self,"removeEventListener",type,old)) return false;
  if (value.isObject() && JS::IsCallable(&value.toObject())) {
    if (!Delegate(s,self,"addEventListener",type,value)) return false;
  } else value.setNull();
  JS::SetReservedSlot(self,slot,value);
  args.rval().setUndefined();return true;
}
}
bool InstallOffscreenEvents(State& s) {
  return s.kit.Accessor(s.offscreen,"OffscreenCanvas","oncontextlost",HandlerGet,HandlerSet,3) &&
         s.kit.Accessor(s.offscreen,"OffscreenCanvas","oncontextrestored",HandlerGet,HandlerSet,4);
}
}
