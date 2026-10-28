#include "execution_scheduling/performance_observer.h"
#include "runtime/exception_capture.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Exception.h"
#include "js/MapAndSet.h"
namespace ruyijs::execution_scheduling {
namespace {
bool ObserverSnapshot(JSContext* cx,JS::HandleObject owner,JS::MutableHandle<JS::GCVector<JS::Value>> observers){
 JSAutoRealm realm(cx,owner);JS::RootedObject set(cx,&JS::GetReservedSlot(owner,PObservers).toObject());
 return js::GetSetObjectKeys(cx,set,observers);
}
bool Deliver(JSContext* cx,unsigned argc,JS::Value* vp){
 auto args=JS::CallArgsFromVp(argc,vp);
 JS::RootedObject owner(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
 JSAutoRealm realm(cx,owner);JS::SetReservedSlot(owner,PObserverPending,JS::BooleanValue(false));
 JS::RootedObject set(cx,&JS::GetReservedSlot(owner,PObservers).toObject());
 JS::RootedValue iterator(cx),next(cx,JS::GetReservedSlot(owner,PObserverNext));
 if(!JS::SetValues(cx,set,&iterator))return false;
 while(true){
  JS::RootedValue step(cx),done(cx),value(cx);
  if(!JS::Call(cx,iterator,next,JS::HandleValueArray::empty(),&step))return false;
  JS::RootedObject row(cx,&step.toObject());
  if(!JS_GetProperty(cx,row,"done",&done))return false;if(done.isTrue())break;
  if(!JS_GetProperty(cx,row,"value",&value))return false;
  JS::RootedObject object(cx,&value.toObject());
  if(!PerfSubscriptionConnected(Subscription(object)))continue;
  JS::RootedObject queue(cx,&JS::GetReservedSlot(object,OQueue).toObject());uint32_t n=0;
  if(!JS::GetArrayLength(cx,queue,&n))return false;if(!n)continue;
  JS::RootedObject empty(cx,JS::NewArrayObject(cx,0));
  JS::RootedObject proto(cx,&JS::GetReservedSlot(owner,PListPrototype).toObject());
  JS::RootedObject list(cx,JS_NewObjectWithGivenProto(cx,&kPerfListClass,proto));
  if(!list||!empty)return false;
  JS::SetReservedSlot(list,0,JS::ObjectValue(*queue));JS::SetReservedSlot(object,OQueue,JS::ObjectValue(*empty));
  JS::RootedValue fn(cx,JS::GetReservedSlot(object,OCallback)),ignored(cx);
  JS::RootedValueArray<2> arguments(cx);arguments[0].setObject(*list);arguments[1].setObject(*object);
  if(!JS::Call(cx,object,fn,arguments,&ignored)){CaptureOperationFailure(cx);JS_ClearPendingException(cx);}
 }
 args.rval().setUndefined();return true;
}
}
bool PerfQueueObserverTask(JSContext* cx,JS::HandleObject owner){
 if(JS::GetReservedSlot(owner,PObserverPending).isTrue())return true;
 JSAutoRealm realm(cx,owner);JS::RootedObject fn(cx,PerfFunction(cx,owner,Deliver,"notifyPerformanceObservers",0,0));
 auto* state=static_cast<State*>(JS::GetReservedSlot(owner,PClock).toPrivate());
 if(!fn||!state->PostTask(fn))return false;
 JS::SetReservedSlot(owner,PObserverPending,JS::BooleanValue(true));return true;
}
bool PerfNotifyEntry(JSContext* cx,JS::HandleObject owner,JS::HandleObject entry){
 JSAutoRealm realm(cx,owner);JS::Rooted<JS::GCVector<JS::Value>> observers(cx,JS::GCVector<JS::Value>(cx));
 if(!ObserverSnapshot(cx,owner,&observers))return false;
 int kind=JS::GetReservedSlot(entry,EKind).toInt32();
 uint32_t type=1u<<(kind==Mark?3:kind==Resource?7:kind==NavigationEntry?5:4);bool queued=false;
 JS::RootedValue value(cx,JS::ObjectValue(*entry));
 for(unsigned i=0;i<observers.length();++i){JS::RootedObject object(cx,&observers[i].toObject());
  if(!PerfSubscriptionObserves(Subscription(object),type))continue;
  JS::RootedObject queue(cx,&JS::GetReservedSlot(object,OQueue).toObject());
  if(!PerfAppend(cx,queue,value))return false;queued=true;
 }
 return !queued||PerfQueueObserverTask(cx,owner);
}
bool PerfCompleteNavigation(JSContext* cx,JS::HandleObject owner){
 JSAutoRealm realm(cx,owner);
 auto* record=PerfNavigationRecord(owner);
 if(!record||record->load_event_finished)return true;
 auto* timeline=static_cast<Timeline*>(JS::GetReservedSlot(owner,PTimeline).toPrivate());
 std::vector<uint32_t> ids;
 if(!SelectTimeline(timeline,nullptr,3,false,&ids)){JS_ReportOutOfMemory(cx);return false;}
 JS::RootedObject map(cx,&JS::GetReservedSlot(owner,PEntries).toObject());
 for(auto id:ids){
  JS::RootedValue value(cx),key(cx,JS::NumberValue(id));
  if(!JS::MapGet(cx,map,key,&value))return false;
  JS::RootedObject entry(cx,&value.toObject());
  JS::SetReservedSlot(entry,EDuration,JS::DoubleValue(record->load_event_end));
  if(!PerfNotifyEntry(cx,owner,entry))return false;
 }
 record->load_event_finished=true;
 return true;
}
}
