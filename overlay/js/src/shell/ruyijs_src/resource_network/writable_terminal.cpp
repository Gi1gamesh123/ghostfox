// Public requests use stream-realm Promises; signal dispatch precedes abort state.
#include "resource_network/writable_stream.h"
#include "resource_network/state.h"
#include "resource_network/abort.h"
#include "js/Promise.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
bool WritableAbort(JSContext* cx,JS::HandleObject stream,JS::HandleValue reason,JS::MutableHandleValue result,CallbackContext context) {
  {
    JSAutoRealm realm(cx,stream);
    auto terminal=[&]{return WritableStatus(stream)==WriteStatus::Closed || WritableStatus(stream)==WriteStatus::Errored;};
    if (terminal()) {if (!Settled(cx,JS::UndefinedHandleValue,result)) return false;}
    else {
      JS::RootedObject controller(cx,SlotObject(stream,WController)),signal(cx,SlotObject(controller,KSignal));
      JS::RootedValue signalReason(cx,reason);
      auto& owner=*static_cast<State*>(JS::GetReservedSlot(signal,AOwner).toPrivate());
      if (!JS_WrapValue(cx,&signalReason) ||
          (signalReason.isUndefined() && !AbortException(owner,"AbortError",&signalReason,"The operation was aborted. ")) ||
          !AbortSignalAbort(owner,signal,signalReason)) return false;
      if (terminal()) {if (!Settled(cx,JS::UndefinedHandleValue,result)) return false;}
      else {
        JS::RootedObject promise(cx,SlotObject(stream,WAbort));
        if (!promise) {
          const bool erroring=WritableStatus(stream)==WriteStatus::Erroring;
          JS::RootedValue stored(cx,erroring?JS::UndefinedValue():reason.get());
          if (!JS_WrapValue(cx,&stored)) return false;
          promise=JS::NewPromiseObject(cx,nullptr);if (!promise) return false;
          JS::SetReservedSlot(stream,WAbort,JS::ObjectValue(*promise));
          JS::SetReservedSlot(stream,WAbortReason,stored);
          JS::SetReservedSlot(stream,WAbortErroring,JS::BooleanValue(erroring));
          if (!erroring) {
            const auto outcome=WritableStartErroring(cx,stream,stored,context);
            if (outcome!=CallbackResult::Success)
              return outcome==CallbackResult::Failure && WritableNativeFailure(cx,controller);
          }
        }
        result.setObject(*promise);
      }
    }
  }
  return JS_WrapValue(cx,result);
}
bool WritableClose(JSContext* cx,JS::HandleObject stream,JS::MutableHandleValue result,CallbackContext context) {
  {
    JSAutoRealm realm(cx,stream);
    if (WritableStatus(stream)==WriteStatus::Closed || WritableStatus(stream)==WriteStatus::Errored) {
      if (!TypeErrorPromise(cx,"Can not close stream after closing or error",result)) return false;
    } else {
      JS::RootedObject controller(cx,SlotObject(stream,WController)),entry(cx);
      JS::RootedObject promise(cx,JS::NewPromiseObject(cx,nullptr));
      if (!promise || !NewWriteEntry(cx,controller,JS::UndefinedHandleValue,0,true,&entry)) return false;
      JS::SetReservedSlot(stream,WClose,JS::ObjectValue(*promise));AppendWriteEntry(controller,entry);
      JS::RootedObject writer(cx,SlotObject(stream,WWriter));
      if (writer && JS::GetReservedSlot(stream,WPressure).toBoolean() && WritableStatus(stream)==WriteStatus::Writable) {
        JS::RootedObject ready(cx,SlotObject(writer,VReady));
        if (!WritablePromise(cx,ready,false,JS::UndefinedHandleValue)) return WritableNativeFailure(cx,controller);
      }
      if (WritableAdvance(cx,controller,context)!=CallbackResult::Success) return false;
      result.setObject(*promise);
    }
  }
  return JS_WrapValue(cx,result);
}
}
