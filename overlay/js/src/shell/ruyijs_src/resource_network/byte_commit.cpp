// BYOB result records belong to reader realm; their typed views retain operation realm.
#include "resource_network/readable_byte.h"
#include "resource_network/state.h"
#include "js/Promise.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
bool ResolveByobRead(JSContext* cx,JS::HandleObject promise,bool done,JS::HandleValue value) {
  JSAutoRealm realm(cx,promise);
  return ResolveRead(cx,promise,done,value);
}
bool PullIntoView(JSContext* cx,JS::HandleObject descriptor,JS::MutableHandleObject result) {
  const auto& data=PullData(descriptor);
  JS::RootedObject previous(cx,SlotObject(descriptor,PBuffer)),buffer(cx);
  return TransferByteBuffer(cx,previous,&buffer) &&
      NewByteView(cx,data.type,buffer,data.offset,data.filled/data.element,result);
}
bool CommitPullInto(JSContext* cx,JS::HandleObject controller,JS::HandleObject descriptor,bool done) {
  JS::RootedObject stream(cx,SlotObject(controller,DStream));
  JS::RootedObject request(cx,SlotObject(stream,SHead)),view(cx);
  if (!request) return Error(cx,"Byte stream read request is missing");
  JS::RootedObject promise(cx,SlotObject(request,QTarget));
  if (!PullIntoView(cx,descriptor,&view)) return ByteCommitFailure(cx,controller);
  JS::RootedValue value(cx,JS::ObjectValue(*view));
  const bool native=IsNativeReadCompletion(promise);
  if (native) PopReadRequest(stream);
  const bool settled=CompleteRead(cx,promise,done,value,PullData(descriptor).kind==ReadKind::Byob);
  if (!settled) {
    if (native) FailNativeReadCompletion(cx,promise);
    return ByteCommitFailure(cx,controller);
  }
  if (!native) PopReadRequest(stream);
  return true;
}
bool RejectPendingException(JSContext* cx,JS::MutableHandleValue result) {
  JS::RootedValue reason(cx);
  if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx,&reason)) return false;
  JS_ClearPendingException(cx);
  return JS_WrapValue(cx,&reason) && Rejected(cx,reason,result);
}
}
