#include "execution_scheduling/performance.h"
#include "jsfriendapi.h"
#include "js/Wrapper.h"
namespace ruyijs::execution_scheduling {
bool PerformanceGlobal(JSContext* cx,unsigned argc,JS::Value* vp){
 auto args=JS::CallArgsFromVp(argc,vp);
 JS::RootedObject original(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
 auto& s=*static_cast<State*>(JS::GetReservedSlot(original,PClock).toPrivate());
 JS::RootedObject receiver(cx,args.thisv().isNullOrUndefined()?&JS::GetReservedSlot(original,PPage).toObject():
  args.thisv().isObject()?js::CheckedUnwrapStatic(&args.thisv().toObject()):nullptr),owner(cx);
 for(JSObject* object:s.performanceObjects){if(&JS::GetReservedSlot(object,PPage).toObject()==receiver){owner=object;break;}}
 if(!owner)return Error(cx,"Illegal Window receiver");
 if(PerfSelector(args)==100){
  if(!argc)return Error(cx,"performance setter requires a value");
  JSAutoRealm realm(cx,receiver);JS::RootedValue value(cx,args[0]);
  if(!JS_WrapValue(cx,&value)||!JS_DefineProperty(cx,receiver,"performance",value,JSPROP_ENUMERATE))return false;
  args.rval().setUndefined();return true;
 }
 args.rval().setObject(*owner);return JS_WrapValue(cx,args.rval());
}
}
