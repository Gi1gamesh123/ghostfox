// Promise handlers retain controller/request through traced slots, never State*.
#include "resource_network/writable_stream.h"
#include "resource_network/state.h"
#include "jsfriendapi.h"
#include "js/Promise.h"
#include "js/Realm.h"
#include <initializer_list>

namespace ruyijs::resource_network {
namespace {
const JSClass kReaction={"Resource sink reaction",JSCLASS_HAS_RESERVED_SLOTS(3)};
bool React(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject record(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
  JS::RootedObject controller(cx,SlotObject(record,0)),extra(cx,SlotObject(record,2));
  const auto kind=SinkReaction(JS::GetReservedSlot(record,1).toInt32());
  const bool reject=js::GetFunctionNativeReserved(&args.callee(),1).toBoolean();
  JS::RootedValue value(cx,args.get(0));args.rval().setUndefined();
  JSAutoRealm realm(cx,controller);
  const auto result=JS_WrapValue(cx,&value)?SinkReact(cx,controller,kind,reject,value,extra):CallbackResult::Failure;
  return result==CallbackResult::Success ||
      (result==CallbackResult::Failure && WritableNativeFailure(cx,controller));
}
}
bool ObserveSink(JSContext* cx,JS::HandleObject controller,JS::HandleObject promise,SinkReaction kind,JS::HandleObject extra) {
  JSAutoRealm realm(cx,controller);
  JS::RootedObject record(cx,JS_NewObjectWithGivenProto(cx,&kReaction,nullptr)),local(cx,promise);
  JS::RootedFunction yes(cx,js::NewFunctionWithReserved(cx,React,1,0,nullptr));
  JS::RootedFunction no(cx,js::NewFunctionWithReserved(cx,React,1,0,nullptr));
  if (!record || !yes || !no || !JS_WrapObject(cx,&local) || !WritableStoreObject(cx,record,2,extra)) return false;
  JS::SetReservedSlot(record,0,JS::ObjectValue(*controller));
  JS::SetReservedSlot(record,1,JS::Int32Value(int(kind)));
  JS::RootedObject fulfilled(cx,JS_GetFunctionObject(yes)),rejected(cx,JS_GetFunctionObject(no));
  for (JSObject* handler:{fulfilled.get(),rejected.get()}) {
    js::SetFunctionNativeReserved(handler,0,JS::ObjectValue(*record));
    js::SetFunctionNativeReserved(handler,1,JS::BooleanValue(handler==rejected));
  }
  return JS::AddPromiseReactions(cx,local,fulfilled,rejected);
}
CallbackResult SinkReact(JSContext* cx,JS::HandleObject controller,SinkReaction kind,bool reject,JS::HandleValue value,JS::HandleObject extra) {
  JS::RootedObject stream(cx,SlotObject(controller,KStream)),promise(cx);
  if (kind==SinkReaction::Start) {
    if (WritableStatus(stream)==WriteStatus::Closed || WritableStatus(stream)==WriteStatus::Errored) return CallbackResult::Success;
    JS::SetReservedSlot(controller,KStarted,JS::BooleanValue(true));
    return reject?WritableDealRejection(cx,stream,value):WritableAdvance(cx,controller);
  }
  if (kind==SinkReaction::Abort)
    return CallbackOutcome(WritablePromise(cx,extra,reject,reject?value:JS::UndefinedHandleValue) && WritableRejectClosed(cx,stream));
  const bool close=kind==SinkReaction::Close;
  const unsigned flight=close?WFlightClose:WFlightWrite;
  promise=SlotObject(stream,flight);
  if (!promise) return CallbackResult::Success; // A native allocation failure already terminated it.
  if (!WritablePromise(cx,promise,reject,reject?value:JS::UndefinedHandleValue)) return CallbackResult::Failure;
  JS::SetReservedSlot(stream,flight,JS::UndefinedValue());
  if (reject) {
    if (!close && WritableStatus(stream)==WriteStatus::Writable) ClearSinkAlgorithms(controller);
    JS::RootedObject abort(cx,close?SlotObject(stream,WAbort):nullptr);
    if (abort) {
      if (!WritablePromise(cx,abort,true,value)) return CallbackResult::Failure;
      JS::SetReservedSlot(stream,WAbort,JS::UndefinedValue());
      JS::SetReservedSlot(stream,WAbortReason,JS::UndefinedValue());
    }
    return WritableDealRejection(cx,stream,value);
  }
  if (close) {
    if (WritableStatus(stream)==WriteStatus::Erroring) {
      JS::RootedObject abort(cx,SlotObject(stream,WAbort));
      if (abort && !WritablePromise(cx,abort,false,JS::UndefinedHandleValue)) return CallbackResult::Failure;
      JS::SetReservedSlot(stream,WAbort,JS::UndefinedValue());
      JS::SetReservedSlot(stream,WAbortReason,JS::UndefinedValue());
      JS::SetReservedSlot(stream,WError,JS::UndefinedValue());
    }
    SetWritableStatus(stream,WriteStatus::Closed);
    JS::RootedObject writer(cx,SlotObject(stream,WWriter));
    if (!writer) return CallbackResult::Success;
    promise=SlotObject(writer,VClosed);return CallbackOutcome(WritablePromise(cx,promise,false,JS::UndefinedHandleValue));
  }
  PopWriteEntry(controller);
  if (WritableStatus(stream)==WriteStatus::Writable && !WritableClosing(stream) &&
      !WritablePressure(cx,stream,WritableDesiredSize(controller)<=0)) return CallbackOutcome(WritableNativeFailure(cx,controller));
  return WritableAdvance(cx,controller);
}
}
