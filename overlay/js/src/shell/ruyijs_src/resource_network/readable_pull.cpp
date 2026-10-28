// Promise reactions retain only the traced controller, never a native State*.
#include "resource_network/readable_controller.h"
#include "resource_network/readable_byte.h"
#include "resource_network/native_source.h"
#include "resource_network/state.h"
#include "jsfriendapi.h"
#include "js/Promise.h"
#include "js/Realm.h"
#include <initializer_list>

namespace ruyijs::resource_network {
namespace {
bool CaptureRejection(JSContext* cx,JS::MutableHandleObject result) {
  JS::RootedValue reason(cx);
  if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx,&reason)) return false;
  JS_ClearPendingException(cx);
  result.set(JS::CallOriginalPromiseReject(cx,reason));
  return result.get()!=nullptr;
}
bool Fulfilled(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject controller(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
  const bool start=js::GetFunctionNativeReserved(&args.callee(),1).toBoolean();
  JSAutoRealm realm(cx,controller);
  int flags=DefaultFlags(controller);
  const bool again=(flags&DPullAgain)!=0;
  flags&=~(DPulling|DPullAgain);
  if (start) flags|=DStarted;
  SetDefaultFlags(controller,flags);
  args.rval().setUndefined();
  return !(start || again) || DefaultPullIfNeeded(cx,controller);
}
bool RejectedCallback(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject controller(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
  JSAutoRealm realm(cx,controller);
  JS::RootedValue reason(cx,args.get(0));
  args.rval().setUndefined();
  return JS_WrapValue(cx,&reason) && DefaultError(cx,controller,reason);
}
bool Observe(JSContext* cx,JS::HandleObject controller,JS::HandleObject promise,bool start) {
  JS::RootedFunction yes(cx,js::NewFunctionWithReserved(cx,Fulfilled,1,0,nullptr));
  JS::RootedFunction no(cx,js::NewFunctionWithReserved(cx,RejectedCallback,1,0,nullptr));
  if (!yes || !no) return false;
  JS::RootedObject fulfilled(cx,JS_GetFunctionObject(yes)),rejected(cx,JS_GetFunctionObject(no));
  for (JSObject* handler : {fulfilled.get(),rejected.get()}) {
    js::SetFunctionNativeReserved(handler,0,JS::ObjectValue(*controller));
    js::SetFunctionNativeReserved(handler,1,JS::BooleanValue(start));
  }
  return JS::AddPromiseReactions(cx,promise,fulfilled,rejected);
}
}
CallbackResult DefaultCallbackPromise(JSContext* cx,JS::HandleObject controller,unsigned slot,
    JS::HandleObject methodGlobal,JS::HandleValue argument,JS::MutableHandleObject result) {
  if (HasNativeSourceAlgorithms(controller)) {
    JS::RootedValue value(cx);
    auto outcome=InvokeNativeSource(cx,controller,slot,methodGlobal,argument,&value);
    if (outcome!=CallbackResult::Success) return outcome;
    result.set(&value.toObject());return CallbackResult::Success;
  }
  JS::RootedValue callback(cx,JS::GetReservedSlot(controller,slot)),value(cx);
  JS::RootedObject target(cx,controller);
  if (!callback.isUndefined()) {
    // Callback entry global belongs to the callable object; a user Proxy keeps
    // its own realm, unlike GetFunctionRealm which follows its target.
    target=js::CheckedUnwrapStatic(&callback.toObject());
    if (!target) return CallbackOutcome(Error(cx,"ReadableStream callback is unavailable"));
  }
  {
    // WebIDL uses the callback entry global, including a foreign callback.
    // An omitted callback creates its intrinsic Promise in the stream realm.
    JSAutoRealm realm(cx,target);
    auto outcome=InvokeDefaultCallback(cx,controller,slot,methodGlobal,argument,&value);
    if (outcome==CallbackResult::EntryRejected) return outcome;
    if (outcome==CallbackResult::Failure) {
      if (!CaptureRejection(cx,result)) return CallbackResult::Failure;
    } else {
      result.set(JS::CallOriginalPromiseResolve(cx,value));
      if (!result && !CaptureRejection(cx,result)) return CallbackResult::Failure;
    }
  }
  return CallbackOutcome(JS_WrapObject(cx,result));
}
bool StartDefaultReadable(JSContext* cx,JS::HandleObject controller) {
  JS::RootedValue argument(cx,JS::ObjectValue(*controller)),value(cx);
  JS::RootedObject methodGlobal(cx,JS::CurrentGlobalOrNull(cx));
  if (InvokeDefaultCallback(cx,controller,DStart,methodGlobal,argument,&value)!=CallbackResult::Success) return false;
  JS::RootedObject promise(cx,JS::NewPromiseObject(cx,nullptr));
  // Donor start uses MaybeSafeResolve: observable then lookup occurs in a job.
  return promise && JS::SafeResolve(cx,promise,value) && Observe(cx,controller,promise,true);
}
bool DefaultPullIfNeeded(JSContext* cx,JS::HandleObject controller) {
  JSAutoRealm realm(cx,controller);
  JS::RootedObject stream(cx,SlotObject(controller,DStream));
  const int flags=DefaultFlags(controller);
  if (ReadableStatus(stream)!=StreamStatus::Readable || (flags&DCloseRequested) || !(flags&DStarted)) return true;
  const double desired=IsByteStream(stream)?ByteDesiredSize(controller):DefaultDesiredSize(controller);
  if (!SlotObject(stream,SHead) && !(desired>0)) return true;
  if (Source(stream)==StreamSource::BufferedBody) return ScheduleRead(cx,stream);
  if (flags&DPulling) {
    SetDefaultFlags(controller,flags|DPullAgain);
    return true;
  }
  SetDefaultFlags(controller,flags|DPulling);
  JS::RootedValue argument(cx,JS::ObjectValue(*controller));
  JS::RootedObject promise(cx);
  JS::RootedObject methodGlobal(cx,JS::CurrentGlobalOrNull(cx));
  return DefaultCallbackPromise(cx,controller,DPull,methodGlobal,argument,&promise)==CallbackResult::Success &&
         Observe(cx,controller,promise,false);
}
}
