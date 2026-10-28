#include "execution_scheduling/performance_observer.h"
#include "runtime/page_helpers.h"
#include "jsfriendapi.h"
#include "js/MapAndSet.h"
namespace ruyijs::execution_scheduling {
bool PerfInstallObservers(JSContext* cx,JS::HandleObject page,JS::HandleObject owner,PageHelpers& helpers) {
 JS::RootedObject observers(cx,JS::NewSetObject(cx));if(!observers)return false;
 JS::SetReservedSlot(owner,PObservers,JS::ObjectValue(*observers));
 JS::SetReservedSlot(owner,PObserverPending,JS::BooleanValue(false));
 JS::RootedValue iterator(cx),next(cx);
 if(!JS::SetValues(cx,observers,&iterator))return false;
 JS::RootedObject cursor(cx,&iterator.toObject());
 if(!JS_GetProperty(cx,cursor,"next",&next))return false;
 JS::SetReservedSlot(owner,PObserverNext,next);
 for(int list=0;list<2;++list){
  const char* iface=list?"PerformanceObserverEntryList":"PerformanceObserver";
  JS::RootedObject prototype(cx,JS_NewPlainObject(cx));
  JS::RootedObject ctor(cx,PerfFunction(cx,owner,list?PerfIllegalConstructor:PerfObserverConstruct,
      iface,list?0:1,0,JSFUN_CONSTRUCTOR));
  if(!prototype||!ctor)return false;
  const char* const observe[]={"observe","disconnect","takeRecords"};
  const char* const query[]={"getEntries","getEntriesByType","getEntriesByName"};
  for(int op=0;op<3;++op){const char* name=list?query[op]:observe[op];
   JS::RootedObject fn(cx,PerfFunction(cx,owner,list?PerfListMethod:PerfObserverMethod,name,list&&op?1:0,op));
   if(!fn||!JS_DefineProperty(cx,prototype,name,fn,JSPROP_ENUMERATE)||!helpers.Member(fn,iface,name,"call"))return false;
  }
  if(!list&&!PerfProperty(cx,helpers,owner,ctor,iface,"supportedEntryTypes",PerfObserverSupported,0))return false;
  if(!PerfFinishInterface(cx,helpers,iface,page,prototype,ctor))return false;
  JS::SetReservedSlot(owner,list?PListPrototype:PObserverPrototype,JS::ObjectValue(*prototype));
 }
 return true;
}
}
