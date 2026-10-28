#include "execution_scheduling/performance.h"
#include "runtime/page_helpers.h"
#include "execution_scheduling/performance_observer.h"
#include "js/MapAndSet.h"
#include "jsfriendapi.h"
#include "interaction/realm.h"
#include "execution_scheduling/navigation_timing.h"
namespace ruyijs::execution_scheduling {
namespace {
void Finalize(JS::GCContext*,JSObject* object) {
 auto value=JS::GetReservedSlot(object,PTimeline);
 if(!value.isUndefined())DeleteTimeline(static_cast<Timeline*>(value.toPrivate()));
 auto timing=JS::GetReservedSlot(object,PNavTiming);
 if(!timing.isUndefined())delete static_cast<NavigationTiming*>(timing.toPrivate());
}
const JSClassOps kOps={nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,Finalize};
}
const JSClass kPerformanceClass={"Performance",JSCLASS_HAS_RESERVED_SLOTS(PerfSlots)|JSCLASS_FOREGROUND_FINALIZE,&kOps};
bool InstallPerformance(State& s,PageHelpers& helpers,interaction::Realm& events){return InstallPerformance(s,s.page,helpers,events);}
bool InstallPerformance(State& s,JS::HandleObject target,PageHelpers& helpers,interaction::Realm& events) {
 JSContext* cx=s.cx;
 JS::RootedObject prototype(cx,JS_NewPlainObject(cx)),owner(cx),constructor(cx),map(cx);
 if(!prototype)return false;
 JS::RootedObject eventPrototype(cx,events.prototype());
 if(!JS_SetPrototype(cx,prototype,eventPrototype))return false;
 owner=JS_NewObjectWithGivenProto(cx,&kPerformanceClass,prototype);
 if(!owner)return false;
 Timeline* timeline=CreateTimeline();if(!timeline){JS_ReportOutOfMemory(cx);return false;}
 JS::SetReservedSlot(owner,PTimeline,JS::PrivateValue(timeline));
 JS::SetReservedSlot(owner,PClock,JS::PrivateValue(&s));
 JS::SetReservedSlot(owner,PEvents,JS::PrivateValue(&events));
 JS::SetReservedSlot(owner,PPage,JS::ObjectValue(*target));
 JS::SetReservedSlot(owner,PHandler,JS::NullValue());
 JS::SetReservedSlot(owner,PNavTiming,JS::UndefinedValue());
 map=JS::NewMapObject(cx);JS::RootedValue exception(cx);
 if(!map||!JS_GetProperty(cx,target,"DOMException",&exception))return false;
 JS::SetReservedSlot(owner,PEntries,JS::ObjectValue(*map));JS::SetReservedSlot(owner,PException,exception);
 JS::RootedValue add(cx),remove(cx),eventConstructor(cx);
 if(!JS_GetProperty(cx,eventPrototype,"addEventListener",&add)||
    !JS_GetProperty(cx,eventPrototype,"removeEventListener",&remove)||
    !JS_GetProperty(cx,eventPrototype,"constructor",&eventConstructor)||!events.Attach(owner))return false;
 JS::SetReservedSlot(owner,PAddListener,add);JS::SetReservedSlot(owner,PRemoveListener,remove);
 if(!PerfInstallEntries(s,target,owner,helpers)||!PerfInstallObservers(cx,target,owner,helpers)||
    (s.exposure==Exposure::Window&&!PerfInstallCounts(cx,target,owner,helpers)))return false;
 const char* const names[]={"now","toJSON","getEntries","getEntriesByType","getEntriesByName",
  "clearResourceTimings","setResourceTimingBufferSize","mark","clearMarks","measure","clearMeasures"};
 const unsigned lengths[]={0,0,0,1,1,0,1,1,0,1,0};
 for(unsigned i=0;i<11;++i){
  JS::RootedObject fn(cx,PerfFunction(cx,owner,PerfMethod,names[i],lengths[i],int(i)));
  if(!fn||!JS_DefineProperty(cx,prototype,names[i],fn,JSPROP_ENUMERATE)||
     !helpers.Member(fn,"Performance",names[i],"call"))return false;
 }
 const char* const properties[]={"timeOrigin","timing","navigation","onresourcetimingbufferfull","eventCounts","interactionCount"};
 const int operations[]={Origin,TimingObject,NavigationObject,Handler,EventCountsObject,InteractionCount};
 for(unsigned i=0;i<6;++i){
  if(s.exposure==Exposure::Worker&&i!=0&&i!=3)continue;
  if(!PerfProperty(cx,helpers,owner,prototype,"Performance",properties[i],PerfMethod,operations[i],i==3))return false;
 }
 constructor=PerfFunction(cx,owner,PerfIllegalConstructor,"Performance",0,0,JSFUN_CONSTRUCTOR);
 JS::RootedObject ctorParent(cx,&eventConstructor.toObject());
 return constructor&&JS_SetPrototype(cx,constructor,ctorParent)&&PerfFinishInterface(cx,helpers,"Performance",target,prototype,constructor)&&
   PerfProperty(cx,helpers,owner,target,s.GlobalInterface(),"performance",PerformanceGlobal,0,true)&&
   helpers.Object("Performance",owner)&&s.performanceObjects.append(owner);
}
}
