#include "execution_scheduling/scheduler.h"
#include "execution_scheduling/performance.h"
#include "interaction/realm.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/ForOfIterator.h"
#include "js/Symbol.h"
#include "js/WeakMap.h"
namespace ruyijs::execution_scheduling {
namespace {
bool HandlerRun(JSContext* cx,unsigned argc,JS::Value* vp){
 auto args=JS::CallArgsFromVp(argc,vp);
 JS::RootedObject record(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
 JS::RootedValue fn(cx,JS::GetReservedSlot(record,THandler)),signal(cx,JS::GetReservedSlot(record,TSignal));
 args.rval().setUndefined();if(!fn.isObject()||!JS::IsCallable(&fn.toObject()))return true;
 return JS::Call(cx,signal,fn,JS::HandleValueArray(args),args.rval());
}
}
bool TaskChange(JSContext* cx,JS::HandleObject record,int priority){
 JSAutoRealm realm(cx,record);JS::RootedObject f(cx,&JS::GetReservedSlot(record,TFactory).toObject());
 if(JS::GetReservedSlot(record,TChanging).isTrue())return TaskException(cx,f,"NotAllowedError");
 int previous=JS::GetReservedSlot(record,TPriority).toInt32();if(previous==priority)return true;
 JS::SetReservedSlot(record,TPriority,JS::Int32Value(priority));JS::SetReservedSlot(record,TChanging,JS::BooleanValue(true));
 struct Guard{JS::HandleObject object;~Guard(){JS::SetReservedSlot(object,TChanging,JS::BooleanValue(false));}}guard{record};
 auto& s=TaskState(f);JS::RootedObject signal(cx,&JS::GetReservedSlot(record,TSignal).toObject());
 for(Callback* cb=s.callbacks.get();cb;cb=cb->next.get()){
  if(cb->kind==Kind::WebTask&&cb->dynamicPriority&&cb->arguments.length()>2&&cb->arguments[2].isObject()){
   JS::RootedObject found(cx);if(!TaskFind(cx,cb->arguments[2],false,&found))return false;
   if(found==record){cb->taskPriority=priority;s.model.PrioritizeWebTask(cb->id,priority);}
  }
 }
 JS::RootedObject proto(cx,&JS::GetReservedSlot(f,TEventProto).toObject());
 auto* events=static_cast<interaction::Realm*>(JS::GetReservedSlot(f,TEvents).toPrivate());
 s.SyncClock();if(!events->FireTaskPriority(signal,proto,kTaskPriorities[previous],s.model.Clock().now))return false;
 JS::RootedObject dependents(cx,&JS::GetReservedSlot(record,TDependents).toObject());uint32_t n=0;
 if(!JS::GetArrayLength(cx,dependents,&n))return false;
 for(uint32_t i=0;i<n;++i){JS::RootedValue value(cx);JS::RootedObject child(cx);
  if(!JS_GetElement(cx,dependents,i,&value)||!TaskFind(cx,value,false,&child))return false;
  if(child&&!TaskChange(cx,child,priority))return false;}
 return true;
}
bool TaskSignalMember(JSContext* cx,unsigned argc,JS::Value* vp){
 auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject record(cx);if(!TaskFind(cx,args.thisv(),false,&record))return false;
 if(!record)return Error(cx,"Illegal TaskSignal receiver");int op=PerfSelector(args);
 if(!op){JS::RootedString text(cx,JS_NewStringCopyZ(cx,kTaskPriorities[JS::GetReservedSlot(record,TPriority).toInt32()]));
  if(!text)return false;args.rval().setString(text);return true;}
 if(op==1){args.rval().set(JS::GetReservedSlot(record,THandler));return JS_WrapValue(cx,args.rval());}
 if(!argc)return Error(cx,"onprioritychange setter requires a value");
 JSAutoRealm realm(cx,record);JS::RootedObject f(cx,&JS::GetReservedSlot(record,TFactory).toObject());
 JS::RootedValue next(cx,args[0]),previous(cx,JS::GetReservedSlot(record,THandler)),signal(cx,JS::GetReservedSlot(record,TSignal));
 if(!JS_WrapValue(cx,&next))return false;if(!next.isObject())next.setNull();
 if(next!=previous){
  JS::RootedValue listener(cx,JS::GetReservedSlot(record,TListener));
  if(next.isObject()&&!listener.isObject()){
   JS::RootedObject fn(cx,TaskFunction(cx,record,HandlerRun,"onprioritychange",1,0));if(!fn)return false;
   listener.setObject(*fn);JS::SetReservedSlot(record,TListener,listener);}
  if(previous.isObject()&&next.isNull()&&!TaskListen(cx,f,signal,"prioritychange",listener,false))return false;
  if(!previous.isObject()&&next.isObject()&&!TaskListen(cx,f,signal,"prioritychange",listener,true))return false;
  JS::SetReservedSlot(record,THandler,next);}
 args.rval().setUndefined();return true;
}
bool TaskSignalAny(JSContext* cx,unsigned argc,JS::Value* vp){
 auto args=JS::CallArgsFromVp(argc,vp);if(!argc)return Error(cx,"TaskSignal.any requires signals");
 JS::RootedObject f(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
 JS::RootedValue any(cx,JS::GetReservedSlot(f,TAbortAny)),signal(cx),priority(cx);JS::RootedObject source(cx);int p=1;
 // Finish WebIDL conversion before allocating a dependent signal. Validation
 // uses the original AbortSignal getter; its state stays in that owner.
 if(!args[0].isObject())return Error(cx,"TaskSignal.any requires a sequence");
 JS::ForOfIterator iter(cx);JS::RootedValueVector sources(cx);JS::RootedValue value(cx),reason(cx);bool done=false,aborted=false;
 if(!iter.init(args[0]))return false;
 while(true){if(!iter.next(&value,&done))return false;if(done)break;
  if(!value.isObject()||!TaskAborted(cx,f,value,&aborted,&reason))return Error(cx,"TaskSignal.any requires signals");
  if(!sources.append(value))return false;}
 if(!PerfOption(cx,args.get(1),"priority",&priority))return false;
 if(!priority.isUndefined()){
  if(!TaskFind(cx,priority,false,&source))return false;
  if(source)p=JS::GetReservedSlot(source,TPriority).toInt32();else if(!TaskPriority(cx,priority,&p))return false;
 }
 JS::RootedObject sequence(cx,JS::NewArrayObject(cx,sources));if(!sequence)return false;
 JS::RootedId iterator(cx,JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx,JS::SymbolCode::iterator)));
 JS::RootedValue values(cx,JS::GetReservedSlot(f,TArrayValues)),input(cx,JS::ObjectValue(*sequence));
 if(!JS_DefinePropertyById(cx,sequence,iterator,values,0)||
    !JS::Call(cx,JS::UndefinedHandleValue,any,JS::HandleValueArray(input),&signal))return false;
 JS::RootedObject object(cx,&signal.toObject()),record(cx);if(!TaskInit(cx,f,object,p,&record))return false;
 JS::SetReservedSlot(record,TFixed,JS::BooleanValue(!source||JS::GetReservedSlot(source,TFixed).toBoolean()));
 if(source&&JS::GetReservedSlot(source,TPrioritySource).isObject()){
  JS::RootedValue origin(cx,JS::GetReservedSlot(source,TPrioritySource));
  if(!TaskFind(cx,origin,false,&source))return false;
 }
 if(source&&JS::GetReservedSlot(source,TFixed).toBoolean())source=nullptr;
 if(source){JS::RootedValue local(cx,JS::GetReservedSlot(source,TSignal));if(!JS_WrapValue(cx,&local))return false;JS::SetReservedSlot(record,TPrioritySource,local);
  JSAutoRealm realm(cx,source);JS::RootedObject list(cx,&JS::GetReservedSlot(source,TDependents).toObject());
  uint32_t n=0;if(!JS_WrapValue(cx,&signal)||!JS::GetArrayLength(cx,list,&n)||!JS_SetElement(cx,list,n,signal))return false;}
 args.rval().setObject(*object);return JS_WrapValue(cx,args.rval());
}
}
