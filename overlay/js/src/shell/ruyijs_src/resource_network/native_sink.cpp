// The original KSink owns one traced operation and one immutable native table.
#include "resource_network/native_sink.h"
#include "resource_network/writable_stream.h"
#include "resource_network/state.h"
#include "js/Promise.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
namespace {
enum Slot { NOperation,NSteps,NSlots };
const JSClass kNativeSink={"Resource native sink algorithms",JSCLASS_HAS_RESERVED_SLOTS(NSlots)};
JSObject* Record(JSObject* controller) {
  JSObject* record=SlotObject(controller,KSink);
  return record && JS::GetClass(record)==&kNativeSink?record:nullptr;
}
}
bool NewNativeSinkAlgorithms(JSContext* cx,JS::HandleObject operation,
    const NativeSinkSteps& steps,JS::MutableHandleObject result) {
  if (!operation || !steps.start || !steps.write || !steps.close || !steps.abort)
    return Error(cx,"Native stream sink requires operation and all steps");
  JS::RootedObject local(cx,operation);
  if (!JS_WrapObject(cx,&local)) return false;
  result.set(JS_NewObjectWithGivenProto(cx,&kNativeSink,nullptr));
  if (!result) return false;
  JS::SetReservedSlot(result,NOperation,JS::ObjectValue(*local));
  JS::SetReservedSlot(result,NSteps,JS::PrivateValue(const_cast<NativeSinkSteps*>(&steps)));
  return true;
}
bool HasNativeSinkAlgorithms(JSObject* controller) {return Record(controller)!=nullptr;}
CallbackResult InvokeNativeSink(JSContext* cx,JS::HandleObject controller,unsigned slot,
    JS::HandleValue argument,JS::MutableHandleValue result,CallbackContext context) {
  JS::RootedObject record(cx,Record(controller));
  if (!record || (slot!=KStart && slot!=KWrite && slot!=KClose && slot!=KAbort)) {
    Error(cx,"Invalid native stream sink operation");return CallbackResult::Failure;
  }
  {
    JSAutoRealm realm(cx,controller);
    JS::RootedObject operation(cx,SlotObject(record,NOperation));
    const auto& steps=*static_cast<const NativeSinkSteps*>(JS::GetReservedSlot(record,NSteps).toPrivate());
    const auto step=slot==KStart?steps.start:slot==KWrite?steps.write:slot==KClose?steps.close:steps.abort;
    JS::RootedValue input(cx,argument);
    if (!JS_WrapValue(cx,&input)) return CallbackResult::Failure;
    const auto outcome=step(cx,operation,controller,input,result,context);
    if (outcome!=CallbackResult::Success) return outcome;
    if (slot!=KStart) {
      JS::RootedObject promise(cx,result.isObject()?js::CheckedUnwrapStatic(&result.toObject()):nullptr);
      if (!promise || !JS::IsPromiseObject(promise)) {
        Error(cx,"Native stream sink algorithm must return an intrinsic Promise");return CallbackResult::Failure;
      }
    }
  }
  return CallbackOutcome(JS_WrapValue(cx,result));
}
}
