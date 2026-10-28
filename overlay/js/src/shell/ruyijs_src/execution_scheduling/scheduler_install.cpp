#include "execution_scheduling/scheduler.h"
#include "execution_scheduling/performance.h"
#include "runtime/page_helpers.h"
#include "interaction/realm.h"
#include "js/PropertyDescriptor.h"
#include "js/WeakMap.h"
#include "js/Realm.h"
namespace ruyijs::execution_scheduling {
const JSClass kSchedulerClass={"Scheduler",JSCLASS_HAS_RESERVED_SLOTS(SSlots)};
const JSClass kTaskFactoryClass={"TaskFactory",JSCLASS_HAS_RESERVED_SLOTS(TFSlots)};
const JSClass kTaskDataClass={"TaskSignalData",JSCLASS_HAS_RESERVED_SLOTS(TDSlots)};
JSObject* TaskFunction(JSContext* cx,JS::HandleObject f,JSNative fn,const char* name,unsigned n,int op,unsigned flags){
 return PerfFunction(cx,f,fn,name,n,op,flags);
}
namespace {
bool Save(JSContext* cx,JS::HandleObject f,unsigned slot,JS::HandleObject from,const char* name,bool getter=false){
 JS::RootedValue value(cx);
 if(getter){JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> d(cx);
  if(!JS_GetOwnPropertyDescriptor(cx,from,name,&d)||d.isNothing())return false;value.setObject(*d->getter());}
 else if(!JS_GetProperty(cx,from,name,&value))return false;
 JS::SetReservedSlot(f,slot,value);return true;
}
}
bool InstallScheduler(State& s,JS::HandleObject page,PageHelpers& helpers,interaction::Realm& events){
 JSContext* cx=s.cx;JSAutoRealm realm(cx,page);
 JS::RootedObject f(cx,JS_NewObjectWithGivenProto(cx,&kTaskFactoryClass,nullptr));if(!f)return false;
 JS::SetReservedSlot(f,TState,JS::PrivateValue(&s));JS::SetReservedSlot(f,TPage,JS::ObjectValue(*page));
 JS::SetReservedSlot(f,TEvents,JS::PrivateValue(&events));
 JS::RootedObject controllers(cx,JS::NewWeakMapObject(cx)),signals(cx,JS::NewWeakMapObject(cx));
 JS::RootedObject eventBase(cx,events.prototype());
 if(!controllers||!signals||!Save(cx,f,TAbortCtor,page,"AbortController")||!Save(cx,f,TException,page,"DOMException")||
  !Save(cx,f,TAdd,eventBase,"addEventListener")||!Save(cx,f,TRemove,eventBase,"removeEventListener"))return false;
 JS::SetReservedSlot(f,TControllerMap,JS::ObjectValue(*controllers));JS::SetReservedSlot(f,TSignalMap,JS::ObjectValue(*signals));
 JS::RootedValue signalCtor(cx),controllerProto(cx),signalProto(cx),eventCtor(cx),eventProto(cx);
 JS::RootedObject abortCtor(cx,&JS::GetReservedSlot(f,TAbortCtor).toObject());
 if(!JS_GetProperty(cx,page,"AbortSignal",&signalCtor)||!JS_GetProperty(cx,abortCtor,"prototype",&controllerProto))return false;
 JS::RootedObject abortSignal(cx,&signalCtor.toObject()),cp(cx,&controllerProto.toObject());
 if(!JS_GetProperty(cx,abortSignal,"prototype",&signalProto))return false;JS::RootedObject sp(cx,&signalProto.toObject());
 if(!Save(cx,f,TAbortGetSignal,cp,"signal",true)||!Save(cx,f,TAbortCall,cp,"abort")||!Save(cx,f,TAbortAny,abortSignal,"any")||
  !Save(cx,f,TAborted,sp,"aborted",true)||!Save(cx,f,TReason,sp,"reason",true)||
  !JS_GetProperty(cx,page,"TaskPriorityChangeEvent",&eventCtor))return false;
 JS::RootedObject ec(cx,&eventCtor.toObject());if(!JS_GetProperty(cx,ec,"prototype",&eventProto))return false;
 JS::SetReservedSlot(f,TEventProto,eventProto);
 JS::RootedObject arrayProto(cx,JS::GetRealmArrayPrototype(cx));
 if(!arrayProto||!Save(cx,f,TArrayValues,arrayProto,"values"))return false;
 for(int kind=0;kind<3;++kind){const char* name=kind==0?"Scheduler":kind==1?"TaskController":"TaskSignal";
  JS::RootedObject proto(cx,JS_NewPlainObject(cx));JS::RootedObject ctor(cx,TaskFunction(cx,f,kind==1?TaskControllerConstruct:PerfIllegalConstructor,name,0,0,JSFUN_CONSTRUCTOR));
  if(!proto||!ctor)return false;
  if(kind==1){if(!JS_SetPrototype(cx,proto,cp)||!JS_SetPrototype(cx,ctor,abortCtor))return false;JS::SetReservedSlot(f,TControllerProto,JS::ObjectValue(*proto));}
  if(kind==2){if(!JS_SetPrototype(cx,proto,sp)||!JS_SetPrototype(cx,ctor,abortSignal))return false;JS::SetReservedSlot(f,TSignalProto,JS::ObjectValue(*proto));}
  if(kind<2){const char* methods[]={kind?"setPriority":"postTask","yield"};
   for(int i=0;i<(kind?1:2);++i){JS::RootedObject fn(cx,TaskFunction(cx,f,kind?TaskSetPriority:SchedulerMethod,methods[i],i?0:1,i));
    if(!fn||!JS_DefineProperty(cx,proto,methods[i],fn,JSPROP_ENUMERATE)||!helpers.Member(fn,name,methods[i],"call"))return false;}}
  else {JS::RootedObject any(cx,TaskFunction(cx,f,TaskSignalAny,"any",1,0));
   if(!any||!JS_DefineProperty(cx,ctor,"any",any,JSPROP_ENUMERATE)||!helpers.Member(any,name,"any","call")||
    !PerfProperty(cx,helpers,f,proto,name,"priority",TaskSignalMember,0)||
    !PerfProperty(cx,helpers,f,proto,name,"onprioritychange",TaskSignalMember,1,true))return false;}
  if(!PerfFinishInterface(cx,helpers,name,page,proto,ctor))return false;
  if(!kind){JS::RootedObject object(cx,JS_NewObjectWithGivenProto(cx,&kSchedulerClass,proto));if(!object)return false;
   JS::SetReservedSlot(object,SFactory,JS::ObjectValue(*f));JS::SetReservedSlot(f,TScheduler,JS::ObjectValue(*object));}
 }
 return PerfProperty(cx,helpers,f,page,s.GlobalInterface(),"scheduler",SchedulerGlobal,0,true)&&s.schedulerFactories.append(f);
}
}
