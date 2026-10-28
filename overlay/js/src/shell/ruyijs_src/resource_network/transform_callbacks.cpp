// Only entered author callback failures are converted to rejected Promises.
#include "resource_network/transform_internal.h"
#include "resource_network/native_transform.h"
#include "resource_network/state.h"
#include "js/CallAndConstruct.h"
#include "js/Exception.h"
#include "js/Promise.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
namespace {
bool Capture(JSContext* cx,JS::MutableHandleObject promise) {
  JS::RootedValue reason(cx);
  if (JS_IsThrowingOutOfMemory(cx) || !JS_IsExceptionPending(cx) || !JS_GetPendingException(cx,&reason)) return false;
  JS_ClearPendingException(cx);promise.set(JS::CallOriginalPromiseReject(cx,reason));return !!promise;
}
}
CallbackResult TransformCall(JSContext* cx,JS::HandleObject controller,JS::HandleValue callback,
    JS::HandleValue chunk,bool withChunk,JS::MutableHandleValue result,CallbackContext context,bool* entered) {
  *entered=false;
  if (callback.isUndefined()) {result.setUndefined();return CallbackResult::Success;}
  JS::RootedObject stream(cx,SlotObject(controller,CTransform)),writable(cx,SlotObject(stream,TWritable));
  JS::RootedObject sink(cx,SlotObject(writable,WController));
  const auto entry=SinkEntry(cx,sink,callback,context);
  if (entry!=CallbackResult::Success) return entry;
  JS::RootedValue function(cx,callback),receiver(cx,JS::GetReservedSlot(controller,CTransformer));
  JS::RootedValueArray<2> args(cx);
  args[0].set(withChunk?chunk.get():JS::ObjectValue(*controller));args[1].setObject(*controller);
  if (!JS_WrapValue(cx,&function) || !JS_WrapValue(cx,&receiver) ||
      !JS_WrapValue(cx,args[0]) || !JS_WrapValue(cx,args[1])) return CallbackResult::Failure;
  *entered=true;
  return CallbackOutcome(JS::Call(cx,receiver,function,JS::HandleValueArray::subarray(args,0,withChunk?2:1),result));
}
CallbackResult TransformCallbackPromise(JSContext* cx,JS::HandleObject controller,unsigned slot,
    JS::HandleValue chunk,JS::MutableHandleObject result,CallbackContext context) {
  if (HasNativeTransformer(controller)) {
    {
      JSAutoRealm realm(cx,controller);
      if (!InvokeNativeTransformer(cx,controller,slot,chunk)) {
        if (!Capture(cx,result)) return CallbackResult::Failure;
      } else result.set(JS::CallOriginalPromiseResolve(cx,JS::UndefinedHandleValue));
      if (!result) return CallbackResult::Failure;
    }
    return CallbackOutcome(JS_WrapObject(cx,result));
  }
  JS::RootedValue callback(cx,JS::GetReservedSlot(controller,slot)),value(cx);
  JS::RootedObject stream(cx,SlotObject(controller,CTransform)),writable(cx,SlotObject(stream,TWritable));
  JS::RootedObject sink(cx,SlotObject(writable,WController));
  const auto entry=SinkEntry(cx,sink,callback,context);
  if (entry!=CallbackResult::Success) return entry;
  JS::RootedObject target(cx,callback.isObject()?js::CheckedUnwrapStatic(&callback.toObject()):controller.get());
  if (!target) {Error(cx,"Transformer callback realm is unavailable");return CallbackResult::Failure;}
  {
    JSAutoRealm realm(cx,target);bool entered=false;
    auto outcome=TransformCall(cx,controller,callback,chunk,slot==CTransformCall,&value,context,&entered);
    if (outcome==CallbackResult::EntryRejected) return outcome;
    if (outcome==CallbackResult::Failure) {
      if (!entered || !Capture(cx,result)) return CallbackResult::Failure;
    } else {
      result.set(JS::CallOriginalPromiseResolve(cx,value));
      if (!result && !Capture(cx,result)) return CallbackResult::Failure;
    }
  }
  return CallbackOutcome(JS_WrapObject(cx,result));
}
CallbackResult PerformTransform(JSContext* cx,JS::HandleObject controller,JS::HandleValue chunk,
    JS::MutableHandleValue result,CallbackContext context) {
  JS::RootedObject promise(cx);
  if (!HasNativeTransformer(controller) && JS::GetReservedSlot(controller,CTransformCall).isUndefined()) {
    if (!TransformEnqueue(cx,controller,chunk)) {
      if (!Capture(cx,&promise)) return CallbackResult::Failure;
    } else promise=JS::CallOriginalPromiseResolve(cx,JS::UndefinedHandleValue);
    if (!promise) return CallbackResult::Failure;
  } else {
    auto outcome=TransformCallbackPromise(cx,controller,CTransformCall,chunk,&promise,context);
    if (outcome!=CallbackResult::Success) return outcome;
  }
  return CallbackOutcome(TransformThen(cx,controller,promise,TransformReaction::Transform,JS::UndefinedHandleValue,result));
}
}
