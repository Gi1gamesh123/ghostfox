#include "resource_network/native_fetch_internal.h"
#include "resource_network/state.h"
#include "resource_network/abort.h"
#include "runtime/exception_capture.h"
#include "jsfriendapi.h"
#include "js/Exception.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
void AttachNativeFetch(State& state,const std::shared_ptr<NativeFetchRecord>& record,
                       JS::HandleObject operation) {
  record->cx=state.cx;record->source=state.asyncSource;
  record->operation.init(state.cx,operation);
  record->next=state.nativeFetches;
  if (record->next) record->next->previous=record.get();
  state.nativeFetches=record.get();
  JS::SetReservedSlot(operation,FRecord,JS::PrivateValue(record.get()));
}
void ClearNativeFetch(State& state,NativeFetchRecord& record,JS::HandleObject operation) {
  if (record.previous) record.previous->next=record.next;
  else state.nativeFetches=record.next;
  if (record.next) record.next->previous=record.previous;
  record.previous=nullptr;record.next=nullptr;
  if (auto* algorithm=SlotObject(operation,FAlgorithm))
    js::SetFunctionNativeReserved(algorithm,0,JS::UndefinedValue());
  for (unsigned slot=0;slot<FOperationSlots;++slot) JS::SetReservedSlot(operation,slot,JS::UndefinedValue());
  record.operation=nullptr;
}
void DiscardNativeFetch(State& state,const std::shared_ptr<NativeFetchRecord>& keep) {
  if (!keep || !keep->operation) return;
  JSContext* cx=state.cx;
  JS::RootedObject operation(cx,keep->operation);
  JSAutoRealm realm(cx,operation);
  JS::RootedValue signal(cx,JS::GetReservedSlot(operation,FSignal));
  JS::RootedValue algorithm(cx,JS::GetReservedSlot(operation,FAlgorithm));
  JS::AutoSaveExceptionState saved(cx);
  ClearNativeFetch(state,*keep,operation);
  CancelNativeFetchTransport(state,*keep);
  if (algorithm.isObject()) (void)RemoveAbortAlgorithm(cx,signal,algorithm);
  saved.restore();
}
bool FinishNativeFetch(State& state,const std::shared_ptr<NativeFetchRecord>& keep,
                       NativeFetchOutcome outcome,const HttpResult* http) {
  if (!keep || !keep->operation) return true;
  JSContext* cx=state.cx;
  JS::RootedObject operation(cx,keep->operation);
  JSAutoRealm realm(cx,operation);
  JS::RootedObject target(cx,SlotObject(operation,FTarget)),factories(cx,SlotObject(operation,FFactories));
  JS::RootedObject global(cx,SlotObject(operation,FGlobal));
  JS::RootedValue signal(cx,JS::GetReservedSlot(operation,FSignal));
  JS::RootedValue algorithm(cx,JS::GetReservedSlot(operation,FAlgorithm));
  const auto* steps=static_cast<const NativeFetchSteps*>(JS::GetReservedSlot(operation,FSteps).toPrivate());
  // Clear all discoverable edges before cancellation, cleanup allocation or reentry.
  ClearNativeFetch(state,*keep,operation);
  CancelNativeFetchTransport(state,*keep);
  if (outcome==NativeFetchOutcome::NativeFailure) {
    JS::AutoSaveExceptionState saved(cx);
    if (algorithm.isObject() && !RemoveAbortAlgorithm(cx,signal,algorithm)) JS_ClearPendingException(cx);
    saved.restore();return FailNativeFetchConsumer(cx,target,*steps);
  }
  if (algorithm.isObject() && !RemoveAbortAlgorithm(cx,signal,algorithm))
    return FailNativeFetchConsumer(cx,target,*steps);
  JS::RootedValue reason(cx);
  JS::RootedObject response(cx);
  if (outcome==NativeFetchOutcome::Aborted &&
      (!AbortReason(state,signal,&reason) || !JS_WrapValue(cx,&reason)))
    return FailNativeFetchConsumer(cx,target,*steps);
  if (outcome==NativeFetchOutcome::Complete &&
      !ResponseObject(state,*http,keep->url,keep->method,keep->mode,
                      keep->credentialsInclude,&response))
    return FailNativeFetchConsumer(cx,target,*steps);
  return steps->finish(cx,target,outcome,response,reason);
}
void DetachNativeFetches(State& state) {
  while (state.nativeFetches) {
    auto keep=state.nativeFetches->weak_from_this().lock();
    JS::RootedObject operation(state.cx,keep->operation);
    ClearNativeFetch(state,*keep,operation);
    CancelNativeFetchTransport(state,*keep);
  }
  // Realm subsequently detaches the whole source, dropping original Job roots.
}
void RetireNativeFetches(State& state) {
  while (state.nativeFetches) {
    auto keep=state.nativeFetches->weak_from_this().lock();
    if (!FinishNativeFetch(state,keep,NativeFetchOutcome::Retired) &&
        CaptureOperationFailure(state.cx)) JS_ClearPendingException(state.cx);
  }
}
}
