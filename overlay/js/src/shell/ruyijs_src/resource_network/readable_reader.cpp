#include "resource_network/readable_stream.h"
#include "resource_network/readable_controller.h"
#include "resource_network/readable_byte.h"
#include "resource_network/state.h"
#include "js/Promise.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
bool ReaderRead(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  State* owner=FunctionOwner(args);
  JS::RootedObject reader(cx,ReaderReceiver(args.thisv()));
  if (!reader) return TypeErrorPromise(cx,"read requires a ReadableStreamDefaultReader",args.rval());
  return ReadDefaultReader(*owner,reader,args.rval());
}
bool ReadDefaultReader(State& owner,JS::HandleObject reader,JS::MutableHandleValue result) {
  JSContext* cx=owner.cx;
  {
    JSAutoRealm realm(cx,reader);
    JS::RootedObject stream(cx,SlotObject(reader,RStream));
    if (!stream) {
      if (!TypeErrorPromise(cx,"Reader has released its lock",result)) return false;
    } else {
      JS::RootedObject promise(cx,JS::NewPromiseObject(cx,nullptr));
      if (!promise) return false;
      if (!ReadDefaultReaderInto(owner,reader,promise)) return false;
      result.setObject(*promise);
    }
  }
  return JS_WrapValue(cx,result);
}
bool ReadDefaultReaderInto(State& owner,JS::HandleObject reader,JS::HandleObject target) {
  JSContext* cx=owner.cx;
  JSAutoRealm realm(cx,reader);
  JS::RootedObject stream(cx,SlotObject(reader,RStream));
  if (!stream) {
    Error(cx,"Reader has released its lock");
    JS::RootedValue reason(cx);
    if (!JS_GetPendingException(cx,&reason)) return false;
    JS_ClearPendingException(cx);
    return RejectReadCompletion(cx,target,reason);
  }
  DisturbReadable(stream);
  if (ReadableStatus(stream)==StreamStatus::Closed)
    return CompleteRead(cx,target,true,JS::UndefinedHandleValue);
  if (ReadableStatus(stream)==StreamStatus::Errored) {
    JS::RootedValue reason(cx,JS::GetReservedSlot(stream,SError));
    return RejectReadCompletion(cx,target,reason);
  }
  if (IsByteStream(stream)) return ByteDefaultRead(cx,stream,target) && ScheduleEnd(owner,stream);
  if (Source(stream)==StreamSource::Default) return DefaultRead(cx,stream,target);
  return QueueRead(cx,stream,target) && ScheduleEnd(owner,stream);
}
bool ReaderClosed(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject reader(cx,ReaderReceiver(args.thisv()));
  if (!reader) return TypeErrorPromise(cx,"closed requires a ReadableStreamDefaultReader",args.rval());
  args.rval().set(JS::GetReservedSlot(reader,RClosed));
  return JS_WrapValue(cx,args.rval());
}
bool ReaderCancel(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject reader(cx,ReaderReceiver(args.thisv()));
  if (!reader) return TypeErrorPromise(cx,"cancel requires a ReadableStreamDefaultReader",args.rval());
  return CancelReader(cx,reader,args.get(0),args.rval());
}
bool CancelReader(JSContext* cx,JS::HandleObject reader,JS::HandleValue reason,JS::MutableHandleValue result) {
  JS::RootedObject methodGlobal(cx,JS::CurrentGlobalOrNull(cx));
  {
    JSAutoRealm realm(cx,reader);
    JS::RootedObject stream(cx,SlotObject(reader,RStream));
    if (!stream) {
      if (!TypeErrorPromise(cx,"Reader has released its lock",result)) return false;
    } else if (CancelReadable(cx,stream,reason,result,methodGlobal)!=CallbackResult::Success) return false;
  }
  return JS_WrapValue(cx,result);
}
bool ReaderRelease(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject reader(cx,ReaderReceiver(args.thisv()));
  if (!reader) return Error(cx,"releaseLock requires a ReadableStreamDefaultReader");
  args.rval().setUndefined();
  return ReleaseReader(cx,reader);
}
bool ReleaseReader(JSContext* cx,JS::HandleObject reader) {
  JSAutoRealm realm(cx,reader);
  JS::RootedObject stream(cx,SlotObject(reader,RStream));
  if (!stream) return true;
  if (IsByteStream(stream)) ByteRelease(SlotObject(stream,SController));
  JS::RootedObject closed(cx,SlotObject(reader,RClosed));
  if (JS::GetPromiseState(closed)!=JS::PromiseState::Pending) {
    closed=JS::NewPromiseObject(cx,nullptr);
    if (!closed) return false;
    JS::SetReservedSlot(reader,RClosed,JS::ObjectValue(*closed));
  }
  const char* message=IsByobReader(reader)?"Releasing lock":"Reader has released its lock";
  if (!JS::SetAnyPromiseIsHandled(cx,closed) ||
      !RejectWithTypeError(cx,closed,message) || !RejectReads(cx,stream,message)) return false;
  JS::SetReservedSlot(stream,SReader,JS::UndefinedValue());
  JS::SetReservedSlot(reader,RStream,JS::UndefinedValue());
  return true;
}
}
