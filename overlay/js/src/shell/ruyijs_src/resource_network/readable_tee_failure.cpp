// A failed published tee operation settles its promise and original owners.
#include "resource_network/readable_tee.h"
#include "resource_network/readable_controller.h"
#include "resource_network/state.h"
#include "runtime/exception_capture.h"
#include "js/Promise.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
bool TeeCompleteMicrotask(JSContext* cx,bool success) {
  if (success) return true;
  // This is a direct native microtask exception, not a rejected read Promise.
  if (!CaptureOperationFailure(cx)) return false;
  JS_ClearPendingException(cx);
  return true;
}
CallbackResult TeeRecoverFailure(JSContext* cx,JS::HandleObject operation,CallbackResult outcome) {
  SetTeeFlags(operation,TeeFlags(operation)&~(TReading|TReadAgain1|TReadAgain2));
  JS::RootedValue original(cx);
  if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx,&original)) return outcome;
  JS_ClearPendingException(cx);
  {
    JSAutoRealm realm(cx,operation);
    JS::RootedValue reason(cx,original);
    if (JS_WrapValue(cx,&reason)) {
      // Reject before canceling the source: its native close step may reenter
      // TeeFinishCancel, which must not fulfill a failed operation's promise.
      JS::RootedObject cancel(cx,SlotObject(operation,TCancelPromise));
      if (!JS::RejectPromise(cx,cancel,reason)) JS_ClearPendingException(cx);
      for (unsigned i=0;i<2;i++) {
        JS::RootedObject branch(cx,SlotObject(operation,i?TBranch2:TBranch1));
        if (!branch) continue;
        JS::RootedObject controller(cx,SlotObject(branch,SController));
        if (!DefaultError(cx,controller,reason)) JS_ClearPendingException(cx);
      }
      JS::RootedObject source(cx,SlotObject(operation,TSource)),global(cx,JS::CurrentGlobalOrNull(cx));
      if (ReadableStatus(source)==StreamStatus::Readable) {
        JS::RootedValue ignored(cx);
        if (CancelReadable(cx,source,reason,&ignored,global)!=CallbackResult::Success)
          JS_ClearPendingException(cx);
        else if (ignored.isObject()) {
          JS::RootedObject promise(cx,js::CheckedUnwrapStatic(&ignored.toObject()));
          if (promise && !JS::SetAnyPromiseIsHandled(cx,promise)) JS_ClearPendingException(cx);
        }
      }
    } else JS_ClearPendingException(cx);
  }
  JS_SetPendingException(cx,original);return outcome;
}
bool TeeOperationFailure(JSContext* cx,JS::HandleObject operation) {
  TeeRecoverFailure(cx,operation,CallbackResult::Failure);return false;
}
}
