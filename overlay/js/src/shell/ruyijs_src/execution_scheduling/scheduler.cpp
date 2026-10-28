#include "execution_scheduling/scheduler.h"
#include "execution_scheduling/performance.h"
#include "jsfriendapi.h"
#include "js/Exception.h"
#include "js/Promise.h"
#include "js/Wrapper.h"
namespace ruyijs::execution_scheduling {
bool SchedulerGlobal(JSContext* cx,unsigned argc,JS::Value* vp){
 auto args=JS::CallArgsFromVp(argc,vp);
 JS::RootedObject original(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
 JS::RootedObject receiver(cx,args.thisv().isNullOrUndefined()?&JS::GetReservedSlot(original,TPage).toObject():
  args.thisv().isObject()?js::CheckedUnwrapStatic(&args.thisv().toObject()):nullptr);
 auto& s=TaskState(original);JS::RootedObject f(cx);
 for(JSObject* raw:s.schedulerFactories){if(&JS::GetReservedSlot(raw,TPage).toObject()==receiver){f=raw;break;}}
 if(!f)return Error(cx,"Illegal Window receiver");
 if(PerfSelector(args)==100){
  if(!argc)return Error(cx,"scheduler setter requires a value");
  JSAutoRealm realm(cx,receiver);JS::RootedValue value(cx,args[0]);
  if(!JS_WrapValue(cx,&value)||!JS_DefineProperty(cx,receiver,"scheduler",value,JSPROP_ENUMERATE))return false;
  args.rval().setUndefined();return true;
 }
 args.rval().set(JS::GetReservedSlot(f,TScheduler));return JS_WrapValue(cx,args.rval());
}
bool SchedulerMethod(JSContext* cx,unsigned argc,JS::Value* vp){
 auto args=JS::CallArgsFromVp(argc,vp);
 JS::RootedObject receiver(cx,args.thisv().isObject()?js::CheckedUnwrapStatic(&args.thisv().toObject()):nullptr);
 bool valid=receiver&&JS::GetClass(receiver)==&kSchedulerClass;
 JS::RootedObject f(cx,valid?&JS::GetReservedSlot(receiver,SFactory).toObject():
  &js::GetFunctionNativeReserved(&args.callee(),0).toObject());
 JS::RootedObject promise(cx);
 {
  JSAutoRealm realm(cx,f);promise=JS::NewPromiseObject(cx,nullptr);if(!promise)return false;
  bool ok=valid?ScheduleWebTask(cx,f,args,PerfSelector(args)==1,promise):Error(cx,"Illegal Scheduler receiver");
  if(!ok){JS::RootedValue error(cx);
   if(!JS_GetPendingException(cx,&error))return false;JS_ClearPendingException(cx);
   if(!JS::RejectPromise(cx,promise,error))return false;}
 }
 args.rval().setObject(*promise);return JS_WrapValue(cx,args.rval());
}
}
