// Native and author sources meet only at the original controller callback port.
#include "resource_network/native_source.h"
#include "resource_network/readable_controller.h"
#include "resource_network/state.h"
#include "js/Promise.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
namespace {
enum Slot { NOperation,NSteps,NSlots };
const JSClass kNativeSource={"Resource native source algorithms",JSCLASS_HAS_RESERVED_SLOTS(NSlots)};
JSObject* SourceRecord(JSObject* controller) {
  JSObject* source=SlotObject(controller,DSource);
  return source && JS::GetClass(source)==&kNativeSource?source:nullptr;
}
}
bool NewNativeSourceAlgorithms(JSContext* cx,JS::HandleObject operation,
    const NativeSourceSteps& steps,JS::MutableHandleObject result) {
  if (!operation || !steps.start || !steps.pull || !steps.cancel)
    return Error(cx,"Native stream source requires operation and all steps");
  JS::RootedObject local(cx,operation);
  if (!JS_WrapObject(cx,&local)) return false;
  result.set(JS_NewObjectWithGivenProto(cx,&kNativeSource,nullptr));
  if (!result) return false;
  JS::SetReservedSlot(result,NOperation,JS::ObjectValue(*local));
  JS::SetReservedSlot(result,NSteps,JS::PrivateValue(const_cast<NativeSourceSteps*>(&steps)));
  return JS_WrapObject(cx,result);
}
bool HasNativeSourceAlgorithms(JSObject* controller) { return SourceRecord(controller)!=nullptr; }
CallbackResult InvokeNativeSource(JSContext* cx,JS::HandleObject controller,unsigned slot,
    JS::HandleObject methodGlobal,JS::HandleValue argument,JS::MutableHandleValue result) {
  JS::RootedObject record(cx,SourceRecord(controller));
  if (!record || (slot!=DStart && slot!=DPull && slot!=DCancel)) {
    Error(cx,"Invalid native stream source operation");return CallbackResult::Failure;
  }
  CallbackResult outcome;
  {
    JSAutoRealm realm(cx,controller);
    JS::RootedObject operation(cx,SlotObject(record,NOperation));
    const auto& steps=*static_cast<const NativeSourceSteps*>(JS::GetReservedSlot(record,NSteps).toPrivate());
    const auto step=slot==DStart?steps.start:slot==DPull?steps.pull:steps.cancel;
    JS::RootedValue input(cx,argument);
    if (!JS_WrapValue(cx,&input)) return CallbackResult::Failure;
    outcome=step(cx,operation,controller,methodGlobal,input,result);
    if (outcome!=CallbackResult::Success) return outcome;
    if (slot!=DStart) {
      JS::RootedObject promise(cx,result.isObject()?js::CheckedUnwrapStatic(&result.toObject()):nullptr);
      if (!promise || !JS::IsPromiseObject(promise)) {
        Error(cx,"Native stream pull and cancel must return an intrinsic Promise");
        return CallbackResult::Failure;
      }
    }
  }
  return CallbackOutcome(JS_WrapValue(cx,result));
}
}
