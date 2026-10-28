#include "resource_network/state.h"
#include "resource_network/abort.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/Exception.h"
namespace ruyijs::resource_network {
namespace {
bool HandlerRun(JSContext* cx,unsigned argc,JS::Value* vp){
 auto args=JS::CallArgsFromVp(argc,vp);
 JS::RootedObject object(cx,&js::GetFunctionNativeReserved(&args.callee(),1).toObject());
 JS::RootedValue fn(cx,JS::GetReservedSlot(object,AHandler));args.rval().setUndefined();
 if(!fn.isObject()||!JS::IsCallable(&fn.toObject()))return true;
 return JS::Call(cx,object,fn,JS::HandleValueArray(args),args.rval());
}
}
bool AbortMember(JSContext* cx,unsigned argc,JS::Value* vp){
 auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject object(cx);
 if(!UnwrapAbort(args.thisv(),&object))return Error(cx,"Illegal AbortSignal receiver");
 auto& s=*static_cast<State*>(JS::GetReservedSlot(object,AOwner).toPrivate());
 int op=js::GetFunctionNativeReserved(&args.callee(),1).toInt32();
 const char* name=op==0?"aborted":op==1?"reason":op==2?"throwIfAborted":"onabort";
 s.Observe("AbortSignal",name,int(op==2?ApiAccess::Call:op==4?ApiAccess::Write:ApiAccess::Read));
 if(op<2){args.rval().set(JS::GetReservedSlot(object,op));return JS_WrapValue(cx,args.rval());}
 if(op==2){args.rval().setUndefined();if(!JS::GetReservedSlot(object,AAborted).toBoolean())return true;
  JS::RootedValue reason(cx,JS::GetReservedSlot(object,AReason));if(!JS_WrapValue(cx,&reason))return false;
  JS_SetPendingException(cx,reason);return false;}
 if(op==3){args.rval().set(JS::GetReservedSlot(object,AHandler));return JS_WrapValue(cx,args.rval());}
 if(!argc)return Error(cx,"onabort setter requires an argument");
 JSAutoRealm realm(cx,object);JS::RootedValue next(cx,args[0]),previous(cx,JS::GetReservedSlot(object,AHandler));
 if(!JS_WrapValue(cx,&next))return false;if(!next.isObject())next.setNull();
 if(next!=previous){
  JS::RootedString type(cx,JS_NewStringCopyZ(cx,"abort"));JS::RootedValueArray<2> values(cx);JS::RootedValue fn(cx),ignored(cx);
  if(!type)return false;values[0].setString(type);
  JS::RootedValue listener(cx,JS::GetReservedSlot(object,AListener));
  if(next.isObject()&&!listener.isObject()){
   JS::RootedObject callback(cx,NativeFunction(s,HandlerRun,"onabort",1));if(!callback)return false;
   js::SetFunctionNativeReserved(callback,1,JS::ObjectValue(*object));listener.setObject(*callback);
   JS::SetReservedSlot(object,AListener,listener);}
  values[1].set(listener);
  if(previous.isObject()&&next.isNull()){fn=s.abortRemove;if(!JS::Call(cx,object,fn,values,&ignored))return false;}
  if(!previous.isObject()&&next.isObject()){fn=s.abortAdd;if(!JS::Call(cx,object,fn,values,&ignored))return false;}
  JS::SetReservedSlot(object,AHandler,next);
 }
 args.rval().setUndefined();return true;
}
}
