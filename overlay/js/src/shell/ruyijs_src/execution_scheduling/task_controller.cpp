#include "execution_scheduling/scheduler.h"
#include "execution_scheduling/performance.h"
#include "runtime/native_constructor_trace.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/WeakMap.h"
namespace ruyijs::execution_scheduling {
bool TaskControllerConstruct(JSContext* cx,unsigned argc,JS::Value* vp){
 auto args=JS::CallArgsFromVp(argc,vp);NativeConstructorTrace trace(cx,args);
 if(!args.isConstructing())return Error(cx,"TaskController requires new");
 JS::RootedObject f(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
 JS::RootedValue priority(cx);int p=1;
 if(!PerfOption(cx,args.get(0),"priority",&priority)||(!priority.isUndefined()&&!TaskPriority(cx,priority,&p)))return false;
 JS::RootedValue ctor(cx,JS::GetReservedSlot(f,TAbortCtor)),signal(cx),getter(cx,JS::GetReservedSlot(f,TAbortGetSignal));
 JS::RootedObject target(cx,&args.newTarget().toObject()),object(cx),record(cx);
 if(!JS::Construct(cx,ctor,target,JS::HandleValueArray::empty(),&object)||
    !JS::Call(cx,object,getter,JS::HandleValueArray::empty(),&signal))return false;
 JS::RootedObject sig(cx,&signal.toObject());if(!TaskInit(cx,f,sig,p,&record))return false;
 JS::RootedObject map(cx,&JS::GetReservedSlot(f,TControllerMap).toObject());
 JS::RootedValue key(cx,JS::ObjectValue(*object)),value(cx,JS::ObjectValue(*record));
 if(!JS::SetWeakMapEntry(cx,map,key,value))return false;
 args.rval().setObject(*object);return trace.Finish(true);
}
bool TaskSetPriority(JSContext* cx,unsigned argc,JS::Value* vp){
 auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject record(cx);int priority=1;
 if(!TaskFind(cx,args.thisv(),true,&record))return false;
 if(!record)return Error(cx,"Illegal TaskController receiver");
 if(!argc||!TaskPriority(cx,args[0],&priority))return argc?false:Error(cx,"setPriority requires a priority");
 args.rval().setUndefined();return TaskChange(cx,record,priority);
}
}
