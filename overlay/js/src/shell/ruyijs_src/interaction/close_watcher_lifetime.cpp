// Original borrowed EventTarget lifetime; no finalizer invokes JS or allocates.
#include "interaction/close_watcher.h"
#include "js/GCAPI.h"
#include <new>

namespace ruyijs::interaction {
namespace {
CloseLifetime* Record(JSObject* object) {
  auto value=JS::GetReservedSlot(object,CLifetime);
  return value.isUndefined()?nullptr:static_cast<CloseLifetime*>(value.toPrivate());
}
void Unlink(CloseLifetime* record) {
  if (!record || !record->owner) return;
  auto& owner=*record->owner;
  if (record->prev) record->prev->next=record->next;
  else owner.closeLifetimes=record->next;
  if (record->next) record->next->prev=record->prev;
  owner.release(record->target);
  record->owner=nullptr;record->target=nullptr;record->prev=record->next=nullptr;
}
void Finalize(JS::GCContext*,JSObject* object) {
  auto* record=Record(object);Unlink(record);delete record;
}
void Trace(JSTracer*,JSObject* object) {
  auto* record=Record(object);
  if (record && record->owner && record->target)
    static_cast<Target*>(record->target)->object=object;
}
const JSClassOps kOps={nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,Finalize,nullptr,nullptr,Trace};
}
const JSClass kCloseWatcherClass={"CloseWatcher",JSCLASS_HAS_RESERVED_SLOTS(CSlots)|JSCLASS_FOREGROUND_FINALIZE,&kOps};
State* CloseOwner(JSObject* object) {
  auto* record=Record(object);return record?record->owner:nullptr;
}
bool AttachCloseWatcher(State& state,JS::HandleObject object) {
  auto* record=new(std::nothrow) CloseLifetime{&state};
  if (!record) {JS_ReportOutOfMemory(state.cx);return false;}
  record->next=state.closeLifetimes;
  if (record->next) record->next->prev=record;
  state.closeLifetimes=record;
  JS::SetReservedSlot(object,CLifetime,JS::PrivateValue(record));
  return state.attach(object,false,&record->target);
}
void DetachCloseWatchers(State& state) {
  while (state.closeLifetimes) Unlink(state.closeLifetimes);
}
}
