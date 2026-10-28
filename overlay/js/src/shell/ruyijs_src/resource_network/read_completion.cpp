// One completion boundary for the original pending-read FIFO and immediate reads.
#include "resource_network/read_completion.h"
#include "resource_network/readable_byte.h"
#include "resource_network/state.h"
#include "js/Promise.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
namespace {
enum Slot { COwner,COperation,CSteps,CSlots };
const JSClass kCompletion={"Resource read completion",JSCLASS_HAS_RESERVED_SLOTS(CSlots)};
bool CallStep(JSContext* cx,JS::HandleObject target,JS::HandleValue input,int kind) {
  JS::RootedObject record(cx,js::CheckedUnwrapStatic(target));
  if (!record) return Error(cx,"Read completion target is unavailable");
  JSAutoRealm realm(cx,record);
  JS::RootedObject owner(cx,SlotObject(record,COwner)),operation(cx,SlotObject(record,COperation));
  const auto& steps=*static_cast<const ReadCompletionSteps*>(JS::GetReservedSlot(record,CSteps).toPrivate());
  JS::RootedValue value(cx,input);
  if (!JS_WrapValue(cx,&value)) return false;
  const auto step=kind==0?steps.chunk:kind==1?steps.close:steps.error;
  return step(cx,owner,operation,value);
}
}
bool NewReadCompletion(JSContext* cx,JS::HandleObject owner,JS::HandleObject operation,
                       const ReadCompletionSteps& steps,JS::MutableHandleObject result) {
  if (!owner || !operation || !steps.chunk || !steps.close || !steps.error)
    return Error(cx,"Read completion requires owner, operation and all native steps");
  JS::RootedObject localOwner(cx,js::CheckedUnwrapStatic(owner));
  if (!localOwner) return Error(cx,"Read completion owner is unavailable");
  JSAutoRealm realm(cx,localOwner);
  JS::RootedObject local(cx,operation);
  if (!JS_WrapObject(cx,&local)) return false;
  result.set(JS_NewObjectWithGivenProto(cx,&kCompletion,nullptr));
  if (!result) return false;
  JS::SetReservedSlot(result,COwner,JS::ObjectValue(*localOwner));
  JS::SetReservedSlot(result,COperation,JS::ObjectValue(*local));
  JS::SetReservedSlot(result,CSteps,JS::PrivateValue(const_cast<ReadCompletionSteps*>(&steps)));
  return true;
}
bool IsNativeReadCompletion(JSObject* target) {
  JSObject* record=target?js::CheckedUnwrapStatic(target):nullptr;
  return record && JS::GetClass(record)==&kCompletion;
}
bool FailNativeReadCompletion(JSContext* cx,JS::HandleObject target) {
  JS::RootedValue reason(cx);
  if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx,&reason)) return false;
  JS_ClearPendingException(cx);
  if (!RejectReadCompletion(cx,target,reason)) return false;
  JS_SetPendingException(cx,reason);return false;
}
bool CompleteRead(JSContext* cx,JS::HandleObject target,bool done,JS::HandleValue value,bool byob) {
  if (IsNativeReadCompletion(target)) return CallStep(cx,target,value,done?1:0);
  return byob?ResolveByobRead(cx,target,done,value):ResolveRead(cx,target,done,value);
}
bool RejectReadCompletion(JSContext* cx,JS::HandleObject target,JS::HandleValue reason) {
  if (IsNativeReadCompletion(target)) return CallStep(cx,target,reason,2);
  JSAutoRealm realm(cx,target);
  JS::RootedValue value(cx,reason);
  return JS_WrapValue(cx,&value) && JS::RejectPromise(cx,target,value);
}
}
