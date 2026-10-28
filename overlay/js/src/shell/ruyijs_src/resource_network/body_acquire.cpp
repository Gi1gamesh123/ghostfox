// One acquisition publication boundary over original holders/readers/tasks.
#include "resource_network/body_acquire_internal.h"
#include "resource_network/state.h"
#include "jsfriendapi.h"
#include "js/Exception.h"
#include "js/GlobalObject.h"
#include "js/Promise.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
bool AcquireBody(JSContext* cx,JS::HandleObject holder,BodyAcquireMode mode,
                   JS::HandleObject target,const BodyAcquireSteps& steps) {
  if (!holder || !target || !steps.finish) return Error(cx,"Body acquisition requires holder, target and native finish");
  JSAutoRealm realm(cx,holder);
  JS::RootedObject factories(cx,SlotObject(holder,HFactories)),localTarget(cx,target);
  State* state=StreamFactoryState(cx,factories);
  if (!state || !JS_WrapObject(cx,&localTarget)) return false;
  JS::RootedObject operation(cx,JS_NewObjectWithGivenProto(cx,&kBodyOperationClass,nullptr));
  if (!operation) return false;
  JS::SetReservedSlot(operation,OHolder,JS::ObjectValue(*holder));
  JS::SetReservedSlot(operation,OTarget,JS::ObjectValue(*localTarget));
  JS::SetReservedSlot(operation,OFactories,JS::ObjectValue(*factories));
  JS::SetReservedSlot(operation,OFinish,JS::PrivateValue(const_cast<BodyAcquireSteps*>(&steps)));
  JS::SetReservedSlot(operation,OPhase,JS::Int32Value(int(BodyPhase::Preparing)));
  BodyRecord* body=HolderBody(holder);
  if (mode==BodyAcquireMode::PublicConsume && (BodyUsed(holder) || BodyLocked(holder))) {
    Error(cx,"Body has already been consumed.");return BodyAcquisitionFailure(cx,operation);
  }
  if (!AttachBodyOperation(*state,operation)) return BodyAcquisitionFailure(cx,operation);
  if (!body->nullBody) body->used=true;
  if (mode==BodyAcquireMode::AfterValidation && BodyLocked(holder))
    return FinishBodyAcquisition(cx,operation,BodyOutcome::Locked);
  JS::RootedObject global(cx,JS::GetNonCCWObjectGlobal(holder));
  if (body->source==BodySource::ScriptStream && state->callbackEntry &&
      !state->callbackEntry(state->callbackEntryContext,global))
    return FinishBodyAcquisition(cx,operation,BodyOutcome::Unavailable);
  JS::RootedObject stream(cx,SlotObject(holder,HStream)),reader(cx);
  if (stream) {
    JS::RootedObject prototype(cx,SlotObject(factories,SFDefaultReader));
    if (!NewDefaultReader(cx,stream,prototype,&reader)) return BodyAcquisitionFailure(cx,operation);
    JS::SetReservedSlot(operation,OReader,JS::ObjectValue(*reader));
  }
  return QueueBodyRead(cx,operation) || BodyAcquisitionFailure(cx,operation);
}
bool QueueBodyRead(JSContext* cx,JS::HandleObject operation) {
  if (BodyTerminal(operation)) return true;
  JS::RootedObject factories(cx,SlotObject(operation,OFactories));
  State* state=StreamFactoryState(cx,factories);
  if (!state) return false;
  JSAutoRealm realm(cx,operation);
  JS::RootedFunction callback(cx,js::NewFunctionWithReserved(cx,ReadBodyTask,0,0,nullptr));
  if (!callback) return false;
  JS::RootedObject task(cx,JS_GetFunctionObject(callback));
  js::SetFunctionNativeReserved(task,0,JS::ObjectValue(*operation));
  if (!state->keepaliveTask) return Error(cx,"Body completion task owner is not bound");
  if (!state->keepaliveTask(state->keepaliveTaskContext,task)) return false;
  JS::SetReservedSlot(operation,OPhase,JS::Int32Value(int(BodyPhase::Queued)));return true;
}
bool FinishBodyAcquisition(JSContext* cx,JS::HandleObject operation,BodyOutcome outcome,JS::HandleValue reason) {
  if (BodyTerminal(operation)) return true;
  JSAutoRealm realm(cx,operation);
  JS::RootedObject holder(cx,SlotObject(operation,OHolder)),target(cx,SlotObject(operation,OTarget));
  JS::RootedObject reader(cx,SlotObject(operation,OReader));
  const auto finish=static_cast<const BodyAcquireSteps*>(JS::GetReservedSlot(operation,OFinish).toPrivate())->finish;
  JS::RootedValue error(cx,reason);
  JS::SetReservedSlot(operation,OPhase,JS::Int32Value(int(BodyPhase::Terminal)));
  UnlinkBodyOperation(operation);
  // Root complete finish inputs first. No continuation retains these strong edges.
  for (unsigned slot=OHolder;slot<=OFinish;++slot) JS::SetReservedSlot(operation,slot,JS::UndefinedValue());
  if (!JS_WrapValue(cx,&error)) return false;
  if (outcome==BodyOutcome::WrongType && reader) {
    JS::RootedValue ignored(cx);
    if (CancelReader(cx,reader,error,&ignored) && ignored.isObject()) {
      JS::RootedObject promise(cx,js::CheckedUnwrapStatic(&ignored.toObject()));
      if (promise) (void)JS::SetAnyPromiseIsHandled(cx,promise);
    }
    // Original FetchStreamReader cleanup discards cancellation failures.
    JS_ClearPendingException(cx);
  }
  return finish(cx,target,holder,outcome,error);
}
bool BodyAcquisitionFailure(JSContext* cx,JS::HandleObject operation) {
  JS::RootedValue reason(cx);
  if (JS_IsExceptionPending(cx)) (void)JS_GetPendingException(cx,&reason);
  JS::AutoSaveExceptionState saved(cx);
  if (!FinishBodyAcquisition(cx,operation,BodyOutcome::NativeFailure,reason)) JS_ClearPendingException(cx);
  saved.restore();
  return false;
}
}
