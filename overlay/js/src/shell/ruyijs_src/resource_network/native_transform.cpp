// Native algorithms occupy the original controller's single transformer slot.
#include "resource_network/native_transform.h"
#include "resource_network/transform_internal.h"
#include "resource_network/state.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
namespace {
enum Slot { NOperation,NSteps,NSlots };
const JSClass kNativeTransformer={"Resource native transformer",JSCLASS_HAS_RESERVED_SLOTS(NSlots)};
JSObject* Record(JSObject* controller) {
  JSObject* record=SlotObject(controller,CTransformer);
  return record && JS::GetClass(record)==&kNativeTransformer?record:nullptr;
}
}
bool NewNativeTransformer(JSContext* cx,JS::HandleObject operation,
    const NativeTransformSteps& steps,JS::MutableHandleObject result) {
  if (!operation || !steps.transform || !steps.flush)
    return Error(cx,"Native transformer requires an operation and both steps");
  JS::RootedObject local(cx,operation);
  if (!JS_WrapObject(cx,&local)) return false;
  result.set(JS_NewObjectWithGivenProto(cx,&kNativeTransformer,nullptr));
  if (!result) return false;
  JS::SetReservedSlot(result,NOperation,JS::ObjectValue(*local));
  JS::SetReservedSlot(result,NSteps,JS::PrivateValue(const_cast<NativeTransformSteps*>(&steps)));
  return true;
}
bool HasNativeTransformer(JSObject* controller) {return Record(controller)!=nullptr;}
bool InvokeNativeTransformer(JSContext* cx,JS::HandleObject controller,unsigned slot,JS::HandleValue chunk) {
  JS::RootedObject record(cx,Record(controller));
  if (!record || (slot!=CTransformCall && slot!=CFlushCall))
    return Error(cx,"Native transformer operation is invalid");
  JSAutoRealm realm(cx,controller);
  JS::RootedObject operation(cx,SlotObject(record,NOperation));
  const auto& steps=*static_cast<const NativeTransformSteps*>(JS::GetReservedSlot(record,NSteps).toPrivate());
  JS::RootedValue input(cx,chunk);
  if (!JS_WrapValue(cx,&input)) return false;
  return (slot==CTransformCall?steps.transform:steps.flush)(cx,operation,controller,input);
}
bool NativeTransformEnqueue(JSContext* cx,JS::HandleObject input,JS::HandleValue chunk) {
  JS::RootedValue value(cx,JS::ObjectValue(*input));
  JS::RootedObject controller(cx,TransformControllerReceiver(value));
  if (!controller) return Error(cx,"Native transformer controller is invalid");
  JSAutoRealm realm(cx,controller);
  JS::RootedValue local(cx,chunk);
  return JS_WrapValue(cx,&local) && TransformEnqueue(cx,controller,local);
}
}
