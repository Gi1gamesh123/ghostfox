// Buffered native source and traced read-request queue. Jobs never retain State*.
#include "resource_network/readable_stream.h"
#include "resource_network/readable_byte.h"
#include "resource_network/state.h"
#include "jsfriendapi.h"
#include "js/ArrayBuffer.h"
#include "js/experimental/TypedData.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include <cstring>
#include <algorithm>

namespace ruyijs::resource_network {
JSObject* PopReadRequest(JSObject* stream) {
  JSObject* first=SlotObject(stream,SHead);
  if (!first) return nullptr;
  auto next=JS::GetReservedSlot(first,QNext);
  JS::SetReservedSlot(stream,SHead,next);
  if (!next.isObject()) JS::SetReservedSlot(stream,STail,JS::UndefinedValue());
  return first;
}
namespace {
bool DeliverBytes(JSContext* cx,JS::HandleObject stream) {
  BodyRecord* body=StreamBody(stream);
  // An empty general stream has no buffered underlying source and stays pending.
  if (!body || !SlotObject(stream,SHead)) return true;
  if (ReadableStatus(stream)!=StreamStatus::Readable || (Flags(stream)&Delivered)) return true;
  JS::RootedObject controller(cx,SlotObject(stream,SController));
  if (body->bytes.empty()) {
    SetFlags(stream,Flags(stream)|Delivered);
    if (!ByteClose(cx,controller,ByteCloseKind::Buffered)) return false;
    return !SlotObject(controller,BPendingHead) || ByteRespond(cx,controller,0);
  }
  // A released reader may leave a live pull-into buffer. Native input still
  // fills that buffer first, preserving the donor's byte-region boundaries.
  size_t produced=0;
  SetFlags(stream,Flags(stream)|Delivered);
  JS::RootedObject descriptor(cx,SlotObject(controller,BPendingHead));
  if (descriptor) {
    auto& data=PullData(descriptor);
    produced=size_t(std::min<uint64_t>(body->bytes.size(),data.capacity-data.filled));
    JS::RootedObject buffer(cx,SlotObject(descriptor,PBuffer));
    if (JS::IsDetachedArrayBufferObject(buffer)) {
      Error(cx,"Detached native Body pull-into buffer");return ByteFailure(cx,controller);
    }
    {
      JS::AutoCheckCannotGC noGC;bool shared=false;
      auto* target=JS::GetArrayBufferData(buffer,&shared,noGC);
      std::memcpy(target+size_t(data.offset+data.filled),body->bytes.data(),produced);
    }
    if (!ByteRespond(cx,controller,produced)) return false;
    if (produced==body->bytes.size()) return true;
  }
  JS::RootedObject bytes(cx,JS_NewUint8Array(cx,body->bytes.size()-produced));
  if (!bytes) return false;
  {
    JS::AutoCheckCannotGC noGC;
    bool shared=false;
    auto* data=JS_GetUint8ArrayData(bytes,&shared,noGC);
    std::memcpy(data,body->bytes.data()+produced,body->bytes.size()-produced);
  }
  JS::RootedValue value(cx,JS::ObjectValue(*bytes));
  // Delivered records source production only. The byte queue is the sole cursor.
  return ByteEnqueue(cx,controller,value);
}
bool Deliver(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject stream(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
  JSAutoRealm realm(cx,stream);
  args.rval().setUndefined();SetFlags(stream,Flags(stream)&~Scheduled);
  if (DeliverBytes(cx,stream)) return true;
  // A failed source emission must not be mistaken for a successful EOF task.
  JS::RootedObject controller(cx,SlotObject(stream,SController));
  return ByteFailure(cx,controller);
}
}
bool ResolveRead(JSContext* cx,JS::HandleObject promise,bool done,JS::HandleValue value) {
  JS::RootedObject result(cx,JS_NewPlainObject(cx));
  JS::RootedValue finished(cx,JS::BooleanValue(done));
  JS::RootedValue chunk(cx,value);
  if (!result || !JS_WrapValue(cx,&chunk) ||
      !JS_DefineProperty(cx,result,"done",finished,JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx,result,"value",chunk,JSPROP_ENUMERATE)) return false;
  JS::RootedValue record(cx,JS::ObjectValue(*result));
  JSAutoRealm realm(cx,promise);
  return JS_WrapValue(cx,&record) && JS::SafeResolve(cx,promise,record);
}
bool PrepareReadRequest(JSContext* cx,JS::HandleObject stream,JS::HandleObject promise,JS::MutableHandleObject request) {
  JSAutoRealm realm(cx,stream);
  JS::RootedObject wrapped(cx,promise);
  if (!JS_WrapObject(cx,&wrapped)) return false;
  request.set(JS_NewObjectWithGivenProto(cx,&kReadRequestClass,nullptr));
  if (!request) return false;
  JS::SetReservedSlot(request,QTarget,JS::ObjectValue(*wrapped));
  return true;
}
void AppendReadRequest(JSObject* stream,JSObject* request) {
  JSObject* tail=SlotObject(stream,STail);
  if (tail) JS::SetReservedSlot(tail,QNext,JS::ObjectValue(*request));
  else JS::SetReservedSlot(stream,SHead,JS::ObjectValue(*request));
  JS::SetReservedSlot(stream,STail,JS::ObjectValue(*request));
}
bool QueueRead(JSContext* cx,JS::HandleObject stream,JS::HandleObject promise) {
  JSAutoRealm realm(cx,stream);
  JS::RootedObject request(cx);
  if (!PrepareReadRequest(cx,stream,promise,&request)) return false;
  AppendReadRequest(stream,request);
  return ScheduleRead(cx,stream);
}
bool ScheduleRead(JSContext* cx,JS::HandleObject stream) {
  if ((Flags(stream)&(Scheduled|Delivered)) || !StreamBody(stream)) return true;
  JS::RootedObject trigger(cx,JS::NewPromiseObject(cx,nullptr));
  JS::RootedFunction callback(cx,js::NewFunctionWithReserved(cx,Deliver,0,0,nullptr));
  if (!trigger || !callback) return false;
  JS::RootedObject handler(cx,JS_GetFunctionObject(callback));
  js::SetFunctionNativeReserved(handler,0,JS::ObjectValue(*stream));
  if (!JS::AddPromiseReactions(cx,trigger,handler,handler)) return false;
  SetFlags(stream,Flags(stream)|Scheduled);
  return JS::ResolvePromise(cx,trigger,JS::UndefinedHandleValue);
}
bool CloseReadable(JSContext* cx,JS::HandleObject stream) {
  if (ReadableStatus(stream)!=StreamStatus::Readable) return true;
  SetReadableStatus(stream,StreamStatus::Closed);
  if (Source(stream)==StreamSource::BufferedBody) SetFlags(stream,Flags(stream)|Delivered);
  if (JSObject* current=SlotObject(stream,SReader)) {
    JS::RootedObject closed(cx,SlotObject(current,RClosed));
    JSAutoRealm realm(cx,closed);
    if (!JS::ResolvePromise(cx,closed,JS::UndefinedHandleValue)) return false;
  }
  JSObject* current=SlotObject(stream,SReader);
  if (current && IsByobReader(current)) return true;
  while (SlotObject(stream,SHead)) {
    JS::RootedObject request(cx,PopReadRequest(stream));
    JS::RootedObject promise(cx,SlotObject(request,QTarget));
    if (!CompleteRead(cx,promise,true,JS::UndefinedHandleValue)) {
      if (!IsNativeReadCompletion(promise)) return false;
      FailNativeReadCompletion(cx,promise);
      JS::RootedValue reason(cx);
      if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx,&reason)) return false;
      JS_ClearPendingException(cx);
      if (!RejectReads(cx,stream,reason)) return false;
      JS_SetPendingException(cx,reason);return false;
    }
  }
  return true;
}
bool RejectReads(JSContext* cx,JS::HandleObject stream,const char* message) {
  if (!SlotObject(stream,SHead)) return true;
  Error(cx,message);
  JS::RootedValue error(cx);
  if (!JS_GetPendingException(cx,&error)) return false;
  JS_ClearPendingException(cx);
  return RejectReads(cx,stream,error);
}
}
