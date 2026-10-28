// Original owner list only finds consumers at retirement; it is not a JS root.
#include "resource_network/body_acquire_internal.h"
#include "resource_network/state.h"
#include "runtime/exception_capture.h"
#include <new>

namespace ruyijs::resource_network {
struct BodyLifetime {
  State* owner=nullptr;
  BodyLifetime* previous=nullptr;
  BodyLifetime* next=nullptr;
  JSObject* object=nullptr;
};
namespace {
void Unlink(BodyLifetime& record) {
  if (!record.owner) return;
  if (record.previous) record.previous->next=record.next;
  else record.owner->bodyOperations=record.next;
  if (record.next) record.next->previous=record.previous;
  record.owner=nullptr;record.previous=nullptr;record.next=nullptr;
}
BodyLifetime* Record(JSObject* object) {
  auto value=JS::GetReservedSlot(object,OLifetime);
  return value.isUndefined()?nullptr:static_cast<BodyLifetime*>(value.toPrivate());
}
void Finalize(JS::GCContext*,JSObject* object) {
  if (auto* record=Record(object)) {Unlink(*record);delete record;}
}
void Trace(JSTracer*,JSObject* object) {
  if (auto* record=Record(object)) record->object=object;
}
const JSClassOps kOps={nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,Finalize,nullptr,nullptr,Trace};
}
const JSClass kBodyOperationClass={"Resource Body acquisition",JSCLASS_HAS_RESERVED_SLOTS(OSlots)|
    JSCLASS_FOREGROUND_FINALIZE,&kOps};
bool AttachBodyOperation(State& state,JS::HandleObject operation) {
  auto* record=new(std::nothrow) BodyLifetime;
  if (!record) {JS_ReportOutOfMemory(state.cx);return false;}
  record->owner=&state;record->object=operation;record->next=state.bodyOperations;
  if (record->next) record->next->previous=record;
  state.bodyOperations=record;
  JS::SetReservedSlot(operation,OLifetime,JS::PrivateValue(record));return true;
}
void UnlinkBodyOperation(JSObject* operation) {
  if (auto* record=Record(operation)) Unlink(*record);
}
void DetachBodyOperations(State& state) {
  while (state.bodyOperations) Unlink(*state.bodyOperations);
}
void RetireBodyConsumers(State& state) {
  while (state.bodyOperations) {
    // A collection can unlink any other record; never retain a bare next link.
    JS::RootedObject operation(state.cx,state.bodyOperations->object);
    UnlinkBodyOperation(operation);
    if (!FinishBodyAcquisition(state.cx,operation,BodyOutcome::Retired)) {
      CaptureOperationFailure(state.cx);JS_ClearPendingException(state.cx);
    }
  }
}
void Realm::RetireBodyConsumers() { if (state_) resource_network::RetireBodyConsumers(*state_); }
}
