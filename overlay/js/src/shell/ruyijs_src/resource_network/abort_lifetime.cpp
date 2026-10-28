// Original Abort factory borrows its event target; signal JS edges stay in slots.
#include "resource_network/abort.h"
#include "resource_network/state.h"
#include "interaction/realm.h"
#include <new>

namespace ruyijs::resource_network {
struct AbortLifetime {
  State* owner=nullptr;
  void* eventRecord=nullptr;
  AbortLifetime* previous=nullptr;
  AbortLifetime* next=nullptr;
};
namespace {
void Unlink(AbortLifetime& record) {
  if (!record.owner) return;
  if (record.previous) record.previous->next=record.next;
  else record.owner->abortLifetimes=record.next;
  if (record.next) record.next->previous=record.previous;
  if (record.eventRecord) record.owner->events->ReleaseBorrowed(record.eventRecord);
  record.owner=nullptr;record.eventRecord=nullptr;record.previous=nullptr;record.next=nullptr;
}
void Finalize(JS::GCContext*,JSObject* object) {
  auto value=JS::GetReservedSlot(object,ALifetime);
  if (value.isUndefined()) return;
  auto* record=static_cast<AbortLifetime*>(value.toPrivate());
  Unlink(*record);delete record;
}
void Trace(JSTracer*,JSObject* object) {
  auto value=JS::GetReservedSlot(object,ALifetime);
  if (value.isUndefined()) return;
  auto* record=static_cast<AbortLifetime*>(value.toPrivate());
  if (record->owner && record->eventRecord)
    record->owner->events->RefreshBorrowed(record->eventRecord,object);
}
const JSClassOps kOps={nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,Finalize,nullptr,nullptr,Trace};
}
const JSClass kAbortSignalClass={"AbortSignal",JSCLASS_HAS_RESERVED_SLOTS(ASlots)|JSCLASS_FOREGROUND_FINALIZE,&kOps};
bool AttachAbortSignal(State& state,JS::HandleObject object) {
  if (!state.events) return Error(state.cx,"AbortSignal event owner is not installed");
  auto* record=new(std::nothrow) AbortLifetime;
  if (!record) {JS_ReportOutOfMemory(state.cx);return false;}
  record->owner=&state;record->next=state.abortLifetimes;
  if (record->next) record->next->previous=record;
  state.abortLifetimes=record;
  JS::SetReservedSlot(object,ALifetime,JS::PrivateValue(record));
  return state.events->AttachBorrowed(object,&record->eventRecord);
}
void DetachAbortSignals(State& state) {
  while (state.abortLifetimes) Unlink(*state.abortLifetimes);
}
}
