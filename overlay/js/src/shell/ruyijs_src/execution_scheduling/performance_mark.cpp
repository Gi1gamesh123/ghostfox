#include "execution_scheduling/performance.h"
#include "runtime/native_constructor_trace.h"
#include "jsfriendapi.h"
namespace ruyijs::execution_scheduling {
bool PerfMark(JSContext* cx,JS::HandleObject owner,const JS::CallArgs& args,
               JS::MutableHandleValue result,bool construct) {
 if(!args.length())return Error(cx,"PerformanceMark requires a name");
 JS::RootedString name(cx);JS::RootedValue detail(cx),startValue(cx),cloned(cx);
 if(!PerfString(cx,args[0],&name)||!PerfOption(cx,args.get(1),"detail",&detail)||
    !PerfOption(cx,args.get(1),"startTime",&startValue))return false;
 double start=0;
 if(startValue.isUndefined())start=PerfNow(owner);
 else if(!PerfNumber(cx,startValue,&start))return false;
 if(PerfReserved(cx,name))return PerfThrow(cx,owner,"SyntaxError");
 if(start<0)return Error(cx,"PerformanceMark startTime must be nonnegative");
 if(!PerfClone(cx,owner,detail,&cloned))return false;
 JS::RootedObject prototype(cx),object(cx);
 if(construct){
  JS::RootedObject target(cx,&args.newTarget().toObject());JS::RootedValue value(cx);
  if(!JS_GetProperty(cx,target,"prototype",&value))return false;
  if(value.isObject())prototype=&value.toObject();
 }
 if(!PerfEntry(cx,owner,Mark,name,start,0,cloned,prototype,&object)||
    (!construct&&!PerfRecord(cx,owner,object)))return false;
 result.setObject(*object);return JS_WrapValue(cx,result);
}
bool PerfMarkConstructor(JSContext* cx,unsigned argc,JS::Value* vp) {
 auto args=JS::CallArgsFromVp(argc,vp);NativeConstructorTrace trace(cx,args);
 if(!args.isConstructing())return Error(cx,"PerformanceMark requires new");
 JS::RootedObject owner(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
 return trace.Finish(PerfMark(cx,owner,args,args.rval(),true));
}
}
