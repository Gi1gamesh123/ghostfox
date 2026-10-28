#include "execution_scheduling/performance_observer.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/web_kit.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/MapAndSet.h"
#include "js/Wrapper.h"
namespace ruyijs::execution_scheduling {
namespace {
void Finalize(JS::GCContext*,JSObject* object){auto slot=JS::GetReservedSlot(object,OSubscription);
 if(!slot.isUndefined())DeletePerfSubscription(static_cast<PerfSubscription*>(slot.toPrivate()));}
const JSClassOps kOps={nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,Finalize};
}
const JSClass kPerfObserverClass={"PerformanceObserver",JSCLASS_HAS_RESERVED_SLOTS(ObserverSlots)|JSCLASS_FOREGROUND_FINALIZE,&kOps};
const JSClass kPerfListClass={"PerformanceObserverEntryList",JSCLASS_HAS_RESERVED_SLOTS(1)};
bool PerfObserverReceiver(JSContext* cx,const JS::CallArgs& args,JS::MutableHandleObject object){
 JS::HandleValue value=args.thisv();
 object.set(value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr);
 return (object&&JS::GetClass(object)==&kPerfObserverClass)||WebKit::FailBrand(cx,args,"PerformanceObserver");
}
bool PerfObserverConstruct(JSContext* cx,unsigned argc,JS::Value* vp){
 auto args=JS::CallArgsFromVp(argc,vp);NativeConstructorTrace trace(cx,args);
 if(!args.isConstructing()||!argc||!args[0].isObject()||!JS::IsCallable(&args[0].toObject()))
  return Error(cx,"PerformanceObserver requires new and a callback");
 JS::RootedObject owner(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
 JS::RootedObject object(cx,JS_NewObjectForConstructor(cx,&kPerfObserverClass,args));
 JS::RootedObject queue(cx,JS::NewArrayObject(cx,0));if(!object||!queue)return false;
 PerfSubscription* model=CreatePerfSubscription();if(!model){JS_ReportOutOfMemory(cx);return false;}
 JS::SetReservedSlot(object,OSubscription,JS::PrivateValue(model));
 JS::SetReservedSlot(object,OOwner,JS::ObjectValue(*owner));JS::SetReservedSlot(object,OCallback,args[0]);
 JS::SetReservedSlot(object,OQueue,JS::ObjectValue(*queue));args.rval().setObject(*object);return trace.Finish(true);
}
bool PerfArrayCopy(JSContext* cx,JS::HandleObject array,JS::MutableHandleObject result){
 uint32_t n=0;{JSAutoRealm realm(cx,array);if(!JS::GetArrayLength(cx,array,&n))return false;}
 result.set(JS::NewArrayObject(cx,n));if(!result)return false;
 for(uint32_t i=0;i<n;++i){JS::RootedValue value(cx);
  {JSAutoRealm realm(cx,array);if(!JS_GetElement(cx,array,i,&value))return false;}
  if(!JS_WrapValue(cx,&value)||!JS_SetElement(cx,result,i,value))return false;
 }return true;
}
bool PerfAppend(JSContext* cx,JS::HandleObject array,JS::HandleValue input){
 JSAutoRealm realm(cx,array);JS::RootedValue value(cx,input);uint32_t n=0;
 return JS_WrapValue(cx,&value)&&JS::GetArrayLength(cx,array,&n)&&JS_SetElement(cx,array,n,value);
}
bool PerfObserverTake(JSContext* cx,JS::HandleObject object,JS::MutableHandleObject result){
 JS::RootedObject queue(cx,&JS::GetReservedSlot(object,OQueue).toObject());
 if(!PerfArrayCopy(cx,queue,result))return false;
 JSAutoRealm realm(cx,object);JS::RootedObject empty(cx,JS::NewArrayObject(cx,0));if(!empty)return false;
 JS::SetReservedSlot(object,OQueue,JS::ObjectValue(*empty));return true;
}
bool PerfConnect(JSContext* cx,JS::HandleObject object,bool connect){
 JSAutoRealm realm(cx,object);JS::RootedObject owner(cx,&JS::GetReservedSlot(object,OOwner).toObject());
 JS::RootedObject set(cx,&JS::GetReservedSlot(owner,PObservers).toObject());
 JS::RootedValue value(cx,JS::ObjectValue(*object));bool removed=false;
 return connect?JS::SetAdd(cx,set,value):JS::SetDelete(cx,set,value,&removed);
}
bool PerfObserverMethod(JSContext* cx,unsigned argc,JS::Value* vp){
 auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject self(cx);if(!PerfObserverReceiver(cx,args,&self))return false;
 switch(PerfSelector(args)){
 case 0:args.rval().setUndefined();return PerfObserve(cx,self,args.get(0));
 case 1:DisconnectPerfSubscription(Subscription(self));args.rval().setUndefined();return PerfConnect(cx,self,false);
 default:JS::RootedObject result(cx);if(!PerfObserverTake(cx,self,&result))return false;args.rval().setObject(*result);return true;
 }
}
bool PerfObserverSupported(JSContext* cx,unsigned argc,JS::Value* vp){
 auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject array(cx,JS::NewArrayObject(cx,0));if(!array)return false;
 for(unsigned i=0;i<8;++i){JS::RootedString name(cx,JS_NewStringCopyZ(cx,kObservedTypes[i]));
  if(!name||!JS_SetElement(cx,array,i,name))return false;}
 args.rval().setObject(*array);return true;
}
}
