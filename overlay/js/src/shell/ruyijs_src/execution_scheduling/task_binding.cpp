#include "execution_scheduling/scheduler.h"
#include "execution_scheduling/performance.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Exception.h"
#include "js/WeakMap.h"
#include "js/Wrapper.h"
namespace ruyijs::execution_scheduling {
bool TaskRead(JSContext* cx,JS::HandleObject f,const char* name,JS::MutableHandleValue out){
 JS::RootedObject page(cx,&JS::GetReservedSlot(f,TPage).toObject());return JS_GetProperty(cx,page,name,out);
}
bool TaskPriority(JSContext* cx,JS::HandleValue value,int* result){
 JS::RootedString text(cx);if(!PerfString(cx,value,&text))return false;
 for(int i=0;i<3;++i){bool equal=false;if(!JS_StringEqualsAscii(cx,text,kTaskPriorities[i],&equal))return false;if(equal){*result=i;return true;}}
 return Error(cx,"Invalid TaskPriority");
}
bool TaskFind(JSContext* cx,JS::HandleValue input,bool controller,JS::MutableHandleObject out){
 JS::RootedObject object(cx,input.isObject()?js::CheckedUnwrapStatic(&input.toObject()):nullptr);out.set(nullptr);
 if(!object)return true;
 for(State* s=activeState;s;s=s->previous){if(s->cx!=cx)continue;
  for(JSObject* raw:s->schedulerFactories){JS::RootedObject f(cx,raw);JSAutoRealm realm(cx,f);
   JS::RootedObject map(cx,&JS::GetReservedSlot(f,controller?TControllerMap:TSignalMap).toObject());
   JS::RootedValue key(cx,JS::ObjectValue(*object)),value(cx);
   if(!JS_WrapValue(cx,&key)||!JS::GetWeakMapEntry(cx,map,key,&value))return false;
   if(value.isObject()){out.set(&value.toObject());return true;}
  }
 }return true;
}
bool TaskInit(JSContext* cx,JS::HandleObject f,JS::HandleObject signal,int priority,JS::MutableHandleObject result){
 JSAutoRealm realm(cx,f);JS::RootedObject proto(cx,&JS::GetReservedSlot(f,TSignalProto).toObject());
 if(!JS_SetPrototype(cx,signal,proto))return false;
 result.set(JS_NewObjectWithGivenProto(cx,&kTaskDataClass,nullptr));
 JS::RootedObject list(cx,JS::NewArrayObject(cx,0)),map(cx,&JS::GetReservedSlot(f,TSignalMap).toObject());
 if(!result||!list)return false;
 JS::SetReservedSlot(result,TFactory,JS::ObjectValue(*f));JS::SetReservedSlot(result,TSignal,JS::ObjectValue(*signal));
 JS::SetReservedSlot(result,TPriority,JS::Int32Value(priority));JS::SetReservedSlot(result,TChanging,JS::BooleanValue(false));
 JS::SetReservedSlot(result,THandler,JS::NullValue());JS::SetReservedSlot(result,TDependents,JS::ObjectValue(*list));
 JS::SetReservedSlot(result,TPrioritySource,JS::NullValue());
 JS::SetReservedSlot(result,TFixed,JS::BooleanValue(false));
 JS::RootedValue key(cx,JS::ObjectValue(*signal)),value(cx,JS::ObjectValue(*result));
 return JS::SetWeakMapEntry(cx,map,key,value);
}
bool TaskException(JSContext* cx,JS::HandleObject f,const char* name){
 JS::RootedValue ctor(cx,JS::GetReservedSlot(f,TException));JS::RootedString text(cx,JS_NewStringCopyZ(cx,name));
 JS::RootedValueArray<2> args(cx);JS::RootedObject error(cx);
 if(!text||!JS_WrapValue(cx,&ctor))return false;args[0].setString(text);args[1].setString(text);
 if(!JS::Construct(cx,ctor,args,&error))return false;JS::RootedValue value(cx,JS::ObjectValue(*error));JS_SetPendingException(cx,value);return false;
}
bool TaskListen(JSContext* cx,JS::HandleObject f,JS::HandleValue signal,const char* name,JS::HandleValue callback,bool add){
 JS::RootedValue fn(cx,JS::GetReservedSlot(f,add?TAdd:TRemove)),ignored(cx);JS::RootedString text(cx,JS_NewStringCopyZ(cx,name));
 JS::RootedValueArray<2> args(cx);if(!text||!JS_WrapValue(cx,&fn))return false;args[0].setString(text);args[1].set(callback);
 return JS::Call(cx,signal,fn,args,&ignored);
}
bool TaskAborted(JSContext* cx,JS::HandleObject f,JS::HandleValue signal,bool* aborted,JS::MutableHandleValue reason){
 *aborted=false;reason.setUndefined();if(signal.isUndefined())return true;
 JS::RootedValue get(cx,JS::GetReservedSlot(f,TAborted)),value(cx);
 if(!JS_WrapValue(cx,&get)||!JS::Call(cx,signal,get,JS::HandleValueArray::empty(),&value))return false;
 *aborted=value.toBoolean();if(!*aborted)return true;get=JS::GetReservedSlot(f,TReason);
 return JS_WrapValue(cx,&get)&&JS::Call(cx,signal,get,JS::HandleValueArray::empty(),reason);
}
}
