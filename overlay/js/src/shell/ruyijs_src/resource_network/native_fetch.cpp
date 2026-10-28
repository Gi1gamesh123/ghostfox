// Internal consumers use the original Request snapshot, transport and Response.
#include "resource_network/native_fetch_internal.h"
#include "resource_network/state.h"
#include "resource_network/abort.h"
#include "runtime/exception_capture.h"
#include "js/Exception.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
const JSClass kNativeFetchClass={"Resource native fetch",JSCLASS_HAS_RESERVED_SLOTS(FOperationSlots)};
bool FetchNative(State& state,const RequestDraft& request,JS::HandleValue signal,
                 JS::HandleObject target,const NativeFetchSteps& steps) {
  JSContext* cx=state.cx;
  if (!target || !steps.finish) return Error(cx,"Native fetch requires target and finish");
  if (!signal.isNullOrUndefined() && !IsAbortSignal(signal)) return Error(cx,"Illegal AbortSignal");
  JSAutoRealm realm(cx,state.page);
  JS::RootedObject localTarget(cx,target),factories(cx);
  JS::RootedValue localSignal(cx,signal);
  if (!JS_WrapObject(cx,&localTarget) || !JS_WrapValue(cx,&localSignal) ||
      !StreamFactories(cx,state.page,&factories)) return false;
  HttpContext context;
  if (!state.resources.ReadHttpContext(&context)) return false;
  HttpOptions options;
  std::shared_ptr<NativeFetchRecord> record;
  if (!Report(cx,PrepareNativeFetch(request,context,&record,&options))) return false;
  JS::RootedObject operation(cx,JS_NewObjectWithGivenProto(cx,&kNativeFetchClass,nullptr));
  if (!operation) return false;
  if (!state.asyncSource && !state.workerScope) return Error(cx,"Native fetch source is not bound");
  JS::SetReservedSlot(operation,FTarget,JS::ObjectValue(*localTarget));
  JS::SetReservedSlot(operation,FFactories,JS::ObjectValue(*factories));
  JS::SetReservedSlot(operation,FGlobal,JS::ObjectValue(*state.page));
  JS::SetReservedSlot(operation,FSignal,localSignal);
  JS::SetReservedSlot(operation,FSteps,JS::PrivateValue(const_cast<NativeFetchSteps*>(&steps)));
  AttachNativeFetch(state,record,operation);
  if (state.callbackEntry && !state.callbackEntry(state.callbackEntryContext,state.page))
    return FinishNativeFetch(state,record,NativeFetchOutcome::Retired);
  if (AbortSignalled(localSignal)) return FinishNativeFetch(state,record,NativeFetchOutcome::Aborted);
  if (!InstallNativeFetchAbort(state,operation)) {
    DiscardNativeFetch(state,record);return false;
  }
  const auto redirect=request.redirect=="manual"?RedirectMode::Manual:
    request.redirect=="error"?RedirectMode::Error:RedirectMode::Follow;
  if (state.workerScope) {
    if (StartWorkerFetch(state,record,request,options,redirect)) return true;
    DiscardNativeFetch(state,record);return false;
  }
  const auto status=StartAsyncHttp(record->source,
    {request.method,request.url,request.headers,request.body,redirect,options},
    {record,NativeFetchComplete},&record->ticket);
  if (status.code!=ErrorCode::None) {
    Report(cx,status);DiscardNativeFetch(state,record);return false;
  }
  return true;
}
bool FailNativeFetchConsumer(JSContext* cx,JS::HandleObject target,const NativeFetchSteps& steps) {
  JS::RootedValue reason(cx);
  if (JS_IsExceptionPending(cx)) (void)JS_GetPendingException(cx,&reason);
  JS::AutoSaveExceptionState saved(cx);
  JS::RootedObject response(cx);
  if (!steps.finish(cx,target,NativeFetchOutcome::NativeFailure,response,reason)) JS_ClearPendingException(cx);
  saved.restore();return false;
}
void NativeFetchComplete(void* target,AsyncHttpResult&& result) noexcept {
  auto& record=*static_cast<NativeFetchRecord*>(target);
  if (!record.operation) return;
  // The Job owns this shared record through completion; reentry can cancel jobs.
  auto keep=record.weak_from_this().lock();
  JSContext* cx=record.cx;
  JS::RootedObject operation(cx,record.operation);
  JSAutoRealm realm(cx,operation);
  JS::RootedObject factories(cx,SlotObject(operation,FFactories));
  State* state=StreamFactoryState(cx,factories);
  NativeFetchOutcome outcome=result.outcome==AsyncHttpOutcome::OutOfMemory?
    NativeFetchOutcome::NativeFailure:result.outcome!=AsyncHttpOutcome::Http || !result.ok?
    NativeFetchOutcome::NetworkFailure:NativeFetchOutcome::Complete;
  if (outcome==NativeFetchOutcome::NativeFailure) JS_ReportOutOfMemory(cx);
  const bool ok=state && FinishNativeFetch(*state,keep,outcome,&result.response);
  if (!ok && CaptureOperationFailure(cx)) JS_ClearPendingException(cx);
}
}
