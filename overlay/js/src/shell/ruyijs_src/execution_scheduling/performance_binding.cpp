#include <cmath>
#include "execution_scheduling/performance.h"
#include "execution_scheduling/navigation_timing.h"
#include "runtime/page_helpers.h"
#include "runtime/web_kit.h"
#include "runtime/native_constructor_trace.h"
#include "jsfriendapi.h"
#include "js/Symbol.h"
#include "js/Wrapper.h"
namespace ruyijs::execution_scheduling {
bool PerfReceiver(JSContext* cx,const JS::CallArgs& args,JS::MutableHandleObject result) {
 JS::HandleValue value=args.thisv();
 result.set(value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr);
 // Cloudflare fingerprints the brand-failure message per member; match Firefox
 // verbatim ("'now' called on an object that does not implement interface
 // Performance.") instead of a fixed "Illegal Performance receiver".
 return (result&&JS::GetClass(result)==&kPerformanceClass)||
        WebKit::FailBrand(cx,args,"Performance");
}
int PerfSelector(const JS::CallArgs& args) {
 return js::GetFunctionNativeReserved(&args.callee(),1).toInt32();
}
// This document's navigation start in epoch milliseconds: the record the load
// coordinator published (docs/owners/NAVIGATION_TIMING.md), or the Context
// epoch for a document that was never navigated (the first one).
double PerfTimeOrigin(JS::HandleObject self) {
 auto* state=static_cast<State*>(JS::GetReservedSlot(self,PClock).toPrivate());
 auto* timing=PerfNavigationRecord(self);
 state->SyncClock();
 if(timing&&timing->present)return timing->time_origin;
 return state->model.Inspect().epoch;
}
JSObject* PerfFunction(JSContext* cx,JS::HandleObject owner,JSNative fn,const char* name,
                       unsigned length,int selector,unsigned flags) {
 JS::RootedFunction f(cx,js::NewFunctionWithReserved(cx,fn,length,flags,name));
 if(!f)return nullptr;
 JS::RootedObject object(cx,JS_GetFunctionObject(f));
 js::SetFunctionNativeReserved(object,0,JS::ObjectValue(*owner));
 js::SetFunctionNativeReserved(object,1,JS::Int32Value(selector));
 return object;
}
bool PerfFinishInterface(JSContext* cx,PageHelpers& helpers,const char* name,
 JS::HandleObject page,JS::HandleObject prototype,JS::HandleObject ctor) {
 JS::RootedString tag(cx,JS_NewStringCopyZ(cx,name));
 JS::RootedId id(cx,JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx,JS::SymbolCode::toStringTag)));
 return tag&&JS_DefineProperty(cx,ctor,"prototype",prototype,JSPROP_READONLY|JSPROP_PERMANENT)&&
  JS_DefineProperty(cx,prototype,"constructor",ctor,0)&&
  JS_DefinePropertyById(cx,prototype,id,tag,JSPROP_READONLY)&&
  JS_DefineProperty(cx,page,name,ctor,0)&&helpers.Interface(name,prototype)&&
  helpers.Constructor(name,ctor);
}
bool PerfProperty(JSContext* cx,PageHelpers& helpers,JS::HandleObject owner,
 JS::HandleObject prototype,const char* iface,const char* name,JSNative fn,int op,bool setter) {
 std::string label=std::string("get ")+name;
 JS::RootedObject get(cx,PerfFunction(cx,owner,fn,label.c_str(),0,op)),set(cx);
 if(setter){label=std::string("set ")+name;set=PerfFunction(cx,owner,fn,label.c_str(),1,op+100);if(!set)return false;}
 return get&&JS_DefineProperty(cx,prototype,name,get,set,JSPROP_ENUMERATE)&&
   helpers.Accessor(iface,prototype,name);
}
bool PerfIllegalConstructor(JSContext* cx,unsigned argc,JS::Value* vp) {
 auto args=JS::CallArgsFromVp(argc,vp);NativeConstructorTrace trace(cx,args);
 return Error(cx,"Illegal constructor.");
}
double PerfNow(JS::HandleObject self) {
 auto* state=static_cast<State*>(JS::GetReservedSlot(self,PClock).toPrivate());
 // Firefox reduces timer precision to 1 ms outside cross-origin isolation.
 state->SyncClock();
 const auto snapshot=state->model.Inspect();
 const double origin=PerfTimeOrigin(self);
 return std::floor(snapshot.epoch+snapshot.now-origin);
}
}
