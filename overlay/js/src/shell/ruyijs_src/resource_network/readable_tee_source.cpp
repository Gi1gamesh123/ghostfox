// One native algorithm table links both original branch controllers to their tee.
#include "resource_network/readable_tee.h"
#include "resource_network/readable_controller.h"
#include "resource_network/state.h"
#include "js/Array.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
namespace {
unsigned Branch(JSObject* operation,JSObject* controller) {
  return SlotObject(controller,DStream)==SlotObject(operation,TBranch1)?0:1;
}
CallbackResult Start(JSContext*,JS::HandleObject,JS::HandleObject,JS::HandleObject,
    JS::HandleValue,JS::MutableHandleValue result) {
  result.setUndefined();return CallbackResult::Success;
}
CallbackResult Pull(JSContext* cx,JS::HandleObject operation,JS::HandleObject controller,
    JS::HandleObject,JS::HandleValue,JS::MutableHandleValue result) {
  if (!TeePull(cx,operation,Branch(operation,controller))) return CallbackResult::Failure;
  if (Settled(cx,JS::UndefinedHandleValue,result)) return CallbackResult::Success;
  return TeeRecoverFailure(cx,operation,CallbackResult::Failure);
}
CallbackResult Cancel(JSContext* cx,JS::HandleObject operation,JS::HandleObject controller,
    JS::HandleObject methodGlobal,JS::HandleValue reason,JS::MutableHandleValue result) {
  const unsigned branch=Branch(operation,controller);
  JS::RootedValue stored(cx,reason);
  if (!JS_WrapValue(cx,&stored)) return TeeRecoverFailure(cx,operation,CallbackResult::Failure);
  JS::SetReservedSlot(operation,branch?TReason2:TReason1,stored);
  SetTeeFlags(operation,TeeFlags(operation)|(branch?TCanceled2:TCanceled1));
  JS::RootedObject cancel(cx,SlotObject(operation,TCancelPromise));
  if (TeeCanceled(operation,0) && TeeCanceled(operation,1)) {
    JS::RootedValue adopted(cx);
    {
      JSAutoRealm realm(cx,methodGlobal);
      JS::RootedObject reasons(cx,JS::NewArrayObject(cx,2));
      JS::RootedValue first(cx,JS::GetReservedSlot(operation,TReason1));
      JS::RootedValue second(cx,JS::GetReservedSlot(operation,TReason2));
      if (!reasons || !JS_WrapValue(cx,&first) || !JS_WrapValue(cx,&second) ||
          !JS_SetElement(cx,reasons,0,first) || !JS_SetElement(cx,reasons,1,second))
        return TeeRecoverFailure(cx,operation,CallbackResult::Failure);
      JS::RootedObject source(cx,SlotObject(operation,TSource));
      JS::RootedValue composite(cx,JS::ObjectValue(*reasons));
      auto outcome=CancelReadable(cx,source,composite,&adopted,methodGlobal);
      if (outcome!=CallbackResult::Success) return TeeRecoverFailure(cx,operation,outcome);
    }
    if (!JS_WrapValue(cx,&adopted) || !JS::SafeResolve(cx,cancel,adopted))
      return TeeRecoverFailure(cx,operation,CallbackResult::Failure);
  }
  result.setObject(*cancel);return CallbackResult::Success;
}
}
const NativeSourceSteps kTeeSourceSteps={Start,Pull,Cancel};
}
