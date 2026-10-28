// One ordered manager graph. Publication/removal/retirement allocate nothing.
#include "interaction/close_watcher.h"

namespace ruyijs::interaction {
namespace {
const JSClass kManager={"Interaction close manager",JSCLASS_HAS_RESERVED_SLOTS(MSlots)};
const JSClass kGroup={"Interaction close group",JSCLASS_HAS_RESERVED_SLOTS(GSlots)};
void Store(JSObject* object,unsigned slot,JSObject* value) {
  JS::SetReservedSlot(object,slot,value?JS::ObjectValue(*value):JS::UndefinedValue());
}
}
bool NewCloseManager(JSContext* cx,JS::MutableHandleObject result) {
  result.set(JS_NewObjectWithGivenProto(cx,&kManager,nullptr));if (!result) return false;
  JS::SetReservedSlot(result,MAllowed,JS::DoubleValue(1));
  JS::SetReservedSlot(result,MNextAllows,JS::BooleanValue(true));
  JS::SetReservedSlot(result,MCount,JS::DoubleValue(0));return true;
}
bool PrepareCloseGroup(JSContext* cx,JS::HandleObject manager,JS::MutableHandleObject group) {
  if (!CloseCanGrow(manager)) {group.set(CloseObject(manager,MTail));return !!group;}
  group.set(JS_NewObjectWithGivenProto(cx,&kGroup,nullptr));return !!group;
}
void PublishCloseWatcher(JSObject* manager,JSObject* group,JSObject* watcher) {
  if (!CloseObject(group,GHead)) {
    JSObject* last=CloseObject(manager,MTail);Store(group,GPrev,last);
    if (last) Store(last,GNext,group);else Store(manager,MHead,group);
    Store(manager,MTail,group);
    JS::SetReservedSlot(manager,MCount,JS::DoubleValue(JS::GetReservedSlot(manager,MCount).toNumber()+1));
  }
  JSObject* last=CloseObject(group,GTail);Store(watcher,CPrev,last);
  if (last) Store(last,CNext,watcher);else Store(group,GHead,watcher);
  Store(group,GTail,watcher);Store(watcher,CManager,manager);Store(watcher,CGroup,group);
  JS::SetReservedSlot(manager,MNextAllows,JS::BooleanValue(true));
}
void RemoveCloseWatcher(JSObject* watcher) {
  JSObject* manager=CloseObject(watcher,CManager),*group=CloseObject(watcher,CGroup);
  if (!manager || !group) return;
  JSObject* prev=CloseObject(watcher,CPrev),*next=CloseObject(watcher,CNext);
  if (prev) Store(prev,CNext,next);else Store(group,GHead,next);
  if (next) Store(next,CPrev,prev);else Store(group,GTail,prev);
  for (unsigned slot:{CManager,CGroup,CPrev,CNext}) Store(watcher,slot,nullptr);
  if (CloseObject(group,GHead)) return;
  prev=CloseObject(group,GPrev);next=CloseObject(group,GNext);
  if (prev) Store(prev,GNext,next);else Store(manager,MHead,next);
  if (next) Store(next,GPrev,prev);else Store(manager,MTail,prev);
  Store(group,GPrev,nullptr);Store(group,GNext,nullptr);
  JS::SetReservedSlot(manager,MCount,JS::DoubleValue(JS::GetReservedSlot(manager,MCount).toNumber()-1));
}
void ClearCloseManager(JSObject* manager) {
  if (!manager) return;
  while (JSObject* group=CloseObject(manager,MHead))
    while (JSObject* watcher=CloseObject(group,GHead)) RemoveCloseWatcher(watcher);
}
bool ProcessCloseManager(State& state) {
  JSContext* cx=state.cx;
  JS::RootedObject manager(cx,state.closeManager),group(cx,manager?CloseObject(manager,MTail):nullptr);
  if (!group || !CloseFullyActive(state)) return true;
  JS::RootedObjectVector snapshot(cx);
  JS::RootedObject cursor(cx,CloseObject(group,GTail));
  while (cursor) {
    if (!snapshot.append(cursor)) return false;
    cursor=CloseObject(cursor,CPrev);
  }
  for (size_t i=0;i<snapshot.length();++i) {
    if (manager!=state.closeManager || !CloseFullyActive(state)) break;
    JS::RootedObject watcher(cx,snapshot[i]);bool proceed=true;
    if (!RequestCloseWatcher(state,watcher,true,&proceed)) return false;
    if (!proceed) break;
  }
  const double allowed=JS::GetReservedSlot(manager,MAllowed).toNumber();
  if (allowed>1) JS::SetReservedSlot(manager,MAllowed,JS::DoubleValue(allowed-1));
  return true;
}
}
