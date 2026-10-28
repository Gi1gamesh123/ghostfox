// Clone the original region before any branch enqueue/response can detach it.
#include "resource_network/readable_tee_byte.h"
#include "resource_network/readable_byte.h"
#include "resource_network/state.h"
#include "js/ArrayBuffer.h"
#include "js/Promise.h"

namespace ruyijs::resource_network {
namespace {
bool Clone(JSContext* cx,JS::HandleValue chunk,JS::MutableHandleValue result) {
  ByteView info;JS::RootedObject buffer(cx),copy(cx),view(cx);
  if (!ConvertByteView(cx,chunk,&info,&buffer,"ReadableStream tee chunk")) return false;
  copy=JS::ArrayBufferClone(cx,buffer,size_t(info.offset),size_t(info.length));
  if (!copy || !NewByteView(cx,JS::Scalar::Uint8,copy,0,info.length,&view)) return false;
  result.setObject(*view);return true;
}
bool Active(JSObject* operation,unsigned i) {
  JSObject* branch=SlotObject(operation,i?TBranch2:TBranch1);
  return !TeeCanceled(operation,i) && ReadableStatus(branch)==StreamStatus::Readable;
}
bool Enqueue(JSContext* cx,JS::HandleObject operation,unsigned i,JS::HandleValue chunk) {
  if (!Active(operation,i)) return true;
  JS::RootedObject branch(cx,SlotObject(operation,i?TBranch2:TBranch1));
  JS::RootedObject controller(cx,SlotObject(branch,SController));
  return ByteEnqueue(cx,controller,chunk);
}
bool Respond(JSContext* cx,JS::HandleObject operation,unsigned i,JS::HandleValue chunk) {
  if (!Active(operation,i)) return true;
  JS::RootedObject branch(cx,SlotObject(operation,i?TBranch2:TBranch1));
  JS::RootedObject controller(cx,SlotObject(branch,SController));
  return ByteRespondNewView(cx,controller,chunk);
}
}
bool TeeByteChunk(JSContext* cx,JS::HandleObject operation,JS::HandleValue chunk,int byob) {
  SetTeeFlags(operation,TeeFlags(operation)&~(TReadAgain1|TReadAgain2));
  JS::RootedValue second(cx,chunk);
  if (byob<0) {
    if (Active(operation,0) && Active(operation,1) && !Clone(cx,chunk,&second))
      return TeeByteFailure(cx,operation);
    if (!Enqueue(cx,operation,0,chunk) || !Enqueue(cx,operation,1,second))
      return TeeOperationFailure(cx,operation);
  } else {
    const unsigned other=1-unsigned(byob);
    if (Active(operation,other) && !Clone(cx,chunk,&second)) return TeeByteFailure(cx,operation);
    if (!Respond(cx,operation,unsigned(byob),chunk) || !Enqueue(cx,operation,other,second))
      return TeeOperationFailure(cx,operation);
  }
  return TeeByteAgain(cx,operation);
}
bool TeeByteFailure(JSContext* cx,JS::HandleObject operation) {
  JS::RootedValue reason(cx),adopted(cx);
  if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx,&reason)) return false;
  const bool oom=JS_IsThrowingOutOfMemory(cx);JS_ClearPendingException(cx);
  for (unsigned i=0;i<2;i++) {
    JS::RootedObject branch(cx,SlotObject(operation,i?TBranch2:TBranch1));
    JS::RootedObject controller(cx,SlotObject(branch,SController));
    if (!DefaultError(cx,controller,reason)) return TeeOperationFailure(cx,operation);
  }
  JS::RootedObject source(cx,SlotObject(operation,TSource)),global(cx,JS::CurrentGlobalOrNull(cx));
  JS::RootedObject cancel(cx,SlotObject(operation,TCancelPromise));
  SetTeeFlags(operation,TeeFlags(operation)&~(TReading|TReadAgain1|TReadAgain2));
  if (CancelReadable(cx,source,reason,&adopted,global)!=CallbackResult::Success ||
      !JS::SafeResolve(cx,cancel,adopted)) return TeeOperationFailure(cx,operation);
  if (oom) {JS_SetPendingException(cx,reason);return false;}
  return true;
}
}
