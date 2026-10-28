#include "execution_scheduling/performance.h"
#include "interaction/realm.h"
#include "js/MapAndSet.h"
#include "jsfriendapi.h"
namespace ruyijs::execution_scheduling {
namespace {
bool Deliver(JSContext* cx,unsigned argc,JS::Value* vp){
 auto args=JS::CallArgsFromVp(argc,vp);
 JS::RootedObject owner(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
 args.rval().setUndefined();JSAutoRealm realm(cx,owner);
 auto* timeline=static_cast<Timeline*>(JS::GetReservedSlot(owner,PTimeline).toPrivate());
 auto* events=static_cast<interaction::Realm*>(JS::GetReservedSlot(owner,PEvents).toPrivate());
 while(size_t before=PendingResources(timeline)){
  if(!ResourceRoom(timeline)&&!events->Fire(owner,"resourcetimingbufferfull",false,false))return false;
  PromoteResources(timeline);
  if(PendingResources(timeline)>=before){
   std::vector<uint32_t> ids;
   if(!DropPendingResources(timeline,&ids)){JS_ReportOutOfMemory(cx);return false;}
   JS::RootedObject map(cx,&JS::GetReservedSlot(owner,PEntries).toObject());
   for(auto id:ids){JS::RootedValue key(cx,JS::NumberValue(id));bool removed;
    if(!JS::MapDelete(cx,map,key,&removed))return false;}
   break;
  }
 }
 FinishResources(timeline);return true;
}
}
bool PerfQueueResourceTask(JSContext* cx,JS::HandleObject owner){
 JSAutoRealm realm(cx,owner);
 JS::RootedObject task(cx,PerfFunction(cx,owner,Deliver,"resourceTimingBufferFull",0,0));
 auto* state=static_cast<State*>(JS::GetReservedSlot(owner,PClock).toPrivate());
 return task&&state->PostTask(task);
}
}
