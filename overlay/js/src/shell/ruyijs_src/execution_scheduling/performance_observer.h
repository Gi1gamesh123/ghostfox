#pragma once
#include "execution_scheduling/performance.h"
#include "execution_scheduling/model_observer.h"
namespace ruyijs::execution_scheduling {
enum ObserverSlot { OOwner, OCallback, OQueue, OSubscription, ObserverSlots };
extern const JSClass kPerfObserverClass,kPerfListClass,kEventCountsClass;
inline constexpr const char* kObservedTypes[]={"event","first-input","largest-contentful-paint",
 "mark","measure","navigation","paint","resource"};
bool PerfInstallObservers(JSContext*,JS::HandleObject,JS::HandleObject,PageHelpers&);
bool PerfInstallCounts(JSContext*,JS::HandleObject,JS::HandleObject,PageHelpers&);
bool PerfObserverConstruct(JSContext*,unsigned,JS::Value*);
bool PerfObserverMethod(JSContext*,unsigned,JS::Value*);
bool PerfObserverSupported(JSContext*,unsigned,JS::Value*);
bool PerfListMethod(JSContext*,unsigned,JS::Value*);
bool PerfCountsMethod(JSContext*,unsigned,JS::Value*);
bool PerfObserverReceiver(JSContext*,const JS::CallArgs&,JS::MutableHandleObject);
bool PerfObserve(JSContext*,JS::HandleObject,JS::HandleValue);
bool PerfConnect(JSContext*,JS::HandleObject,bool);
bool PerfNotifyEntry(JSContext*,JS::HandleObject,JS::HandleObject);
bool PerfCompleteNavigation(JSContext*,JS::HandleObject);
bool PerfQueueObserverTask(JSContext*,JS::HandleObject);
bool PerfObserverTake(JSContext*,JS::HandleObject,JS::MutableHandleObject);
bool PerfArrayCopy(JSContext*,JS::HandleObject,JS::MutableHandleObject);
bool PerfAppend(JSContext*,JS::HandleObject,JS::HandleValue);
inline PerfSubscription* Subscription(JS::HandleObject object){return static_cast<PerfSubscription*>(JS::GetReservedSlot(object,OSubscription).toPrivate());}
}
