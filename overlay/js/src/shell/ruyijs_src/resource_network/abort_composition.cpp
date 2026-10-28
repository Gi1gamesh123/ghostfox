#include "resource_network/state.h"
#include "resource_network/abort.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/ForOfIterator.h"
#include <cmath>
#include <algorithm>
namespace ruyijs::resource_network {
namespace {
bool Append(JSContext* cx,JS::HandleObject target,unsigned slot,JS::HandleValue input){
 JSAutoRealm realm(cx,target);JS::RootedValue value(cx,input);uint32_t n=0;
 JS::RootedObject array(cx,&JS::GetReservedSlot(target,slot).toObject());
 return JS_WrapValue(cx,&value)&&JS::GetArrayLength(cx,array,&n)&&JS_SetElement(cx,array,n,value);
}
bool TimeoutRun(JSContext* cx,unsigned argc,JS::Value* vp){
 auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject signal(cx,&js::GetFunctionNativeReserved(&args.callee(),1).toObject());
 auto& s=*static_cast<State*>(JS::GetReservedSlot(signal,AOwner).toPrivate());
 JSAutoRealm realm(cx,signal);JS::RootedValue reason(cx);
 args.rval().setUndefined();return AbortException(s,"TimeoutError",&reason)&&AbortSignalAbort(s,signal,reason);
}
}
bool AbortCompose(State& s,JS::HandleValue input,JS::MutableHandleObject result){
 JSContext* cx=s.cx;if(!input.isObject())return Error(cx,"AbortSignal.any requires a sequence");
 JS::ForOfIterator iter(cx);if(!iter.init(input))return false;JS::RootedValueVector sources(cx);
 JS::RootedValue value(cx);bool done=false;
 while(true){if(!iter.next(&value,&done))return false;if(done)break;
  if(!IsAbortSignal(value))return Error(cx,"AbortSignal.any requires signals");if(!sources.append(value))return false;}
 if(!NewAbortSignal(s,result))return false;
 JS::RootedValue target(cx,JS::ObjectValue(*result));
 for(unsigned i=0;i<sources.length();++i){JS::RootedObject source(cx);if(!UnwrapAbort(sources[i],&source))return false;
  if(JS::GetReservedSlot(source,AAborted).toBoolean()){
   JS::RootedValue reason(cx,JS::GetReservedSlot(source,AReason));return AbortSignalAbort(s,result,reason);}}
 JS::RootedObjectVector roots(cx);
 for(unsigned i=0;i<sources.length();++i){JS::RootedObject source(cx);if(!UnwrapAbort(sources[i],&source))return false;
  JSAutoRealm realm(cx,source);JS::RootedObject list(cx,&JS::GetReservedSlot(source,ASources).toObject());uint32_t n=0;
  if(!JS::GetArrayLength(cx,list,&n))return false;
  if(!n){if(!roots.append(source))return false;}
  for(uint32_t j=0;j<n;++j){JS::RootedValue value(cx);JS::RootedObject root(cx);
   if(!JS_GetElement(cx,list,j,&value)||!UnwrapAbort(value,&root)||!roots.append(root))return false;}
 }
 for(unsigned i=0;i<roots.length();++i){bool duplicate=false;
  for(unsigned j=0;j<i;++j)if(roots[i]==roots[j]){duplicate=true;break;}if(duplicate)continue;
  JS::RootedObject source(cx,roots[i]);JS::RootedValue value(cx,JS::ObjectValue(*source));
  if(!Append(cx,source,ADependents,target)||!Append(cx,result,ASources,value))return false;}
 return true;
}
bool AbortTimeout(State& s,JS::HandleValue value,JS::MutableHandleObject result){
 double delay=0;if(!JS::ToNumber(s.cx,value,&delay))return false;
 if(!std::isfinite(delay)||std::trunc(delay)<0||std::trunc(delay)>=18446744073709551616.0)
  return Error(s.cx,"Timeout is outside unsigned long long range");
 if(!NewAbortSignal(s,result))return false;
 JS::RootedObject fn(s.cx,NativeFunction(s,TimeoutRun,"abortTimeout",0));if(!fn)return false;
 js::SetFunctionNativeReserved(fn,1,JS::ObjectValue(*result));
 return s.abortDelay?s.abortDelay(s.abortDelayContext,fn,std::min(2147483647.0,std::trunc(delay))):Error(s.cx,"Abort timeout scheduler is not installed");
}
bool AbortStaticMethod(JSContext* cx,unsigned argc,JS::Value* vp){
 auto args=JS::CallArgsFromVp(argc,vp);auto& s=*FunctionOwner(args);
 int op=js::GetFunctionNativeReserved(&args.callee(),1).toInt32();JS::RootedObject signal(cx);
 s.Observe("AbortSignal",op==0?"abort":op==1?"timeout":"any",int(ApiAccess::Call));
 if(op&& !argc)return Error(cx,"AbortSignal method requires an argument");
 if(op==2){if(!AbortCompose(s,args[0],&signal))return false;}
 else if(op==1){if(!AbortTimeout(s,args[0],&signal))return false;}
 else {JS::RootedValue reason(cx,args.get(0));if(!NewAbortSignal(s,&signal))return false;
  if(reason.isUndefined()&&!AbortDefaultReason(s,&reason))return false;
  if(!AbortSignalAbort(s,signal,reason))return false;}
 args.rval().setObject(*signal);return true;
}
}
