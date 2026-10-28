// Chunk and request FIFOs have one owner and prepare before linking.
#include "resource_network/writable_stream.h"
#include "resource_network/state.h"
#include "js/Promise.h"
#include "js/Realm.h"
#include <algorithm>

namespace ruyijs::resource_network {
bool NewWriteEntry(JSContext* cx,JS::HandleObject controller,JS::HandleValue chunk,double size,bool close,JS::MutableHandleObject entry) {
  JSAutoRealm realm(cx,controller);JS::RootedValue local(cx,chunk);
  if (!JS_WrapValue(cx,&local)) return false;
  entry.set(JS_NewObjectWithGivenProto(cx,&kWriteEntryClass,nullptr));if (!entry) return false;
  JS::SetReservedSlot(entry,EChunk,local);JS::SetReservedSlot(entry,ESize,JS::DoubleValue(size));
  JS::SetReservedSlot(entry,EClose,JS::BooleanValue(close));return true;
}
void AppendWriteEntry(JSObject* controller,JSObject* entry) {
  if (JSObject* tail=SlotObject(controller,KTail)) JS::SetReservedSlot(tail,ENext,JS::ObjectValue(*entry));
  else JS::SetReservedSlot(controller,KHead,JS::ObjectValue(*entry));
  JS::SetReservedSlot(controller,KTail,JS::ObjectValue(*entry));
  JS::SetReservedSlot(controller,KTotal,JS::DoubleValue(JS::GetReservedSlot(controller,KTotal).toNumber()+JS::GetReservedSlot(entry,ESize).toNumber()));
}
void PopWriteEntry(JSObject* controller) {
  JSObject* entry=SlotObject(controller,KHead);auto next=JS::GetReservedSlot(entry,ENext);
  JS::SetReservedSlot(controller,KHead,next);if (!next.isObject()) JS::SetReservedSlot(controller,KTail,JS::UndefinedValue());
  JS::SetReservedSlot(controller,KTotal,JS::DoubleValue(std::max(0.0,JS::GetReservedSlot(controller,KTotal).toNumber()-JS::GetReservedSlot(entry,ESize).toNumber())));
}
bool NewWriteRequest(JSContext* cx,JS::HandleObject stream,JS::MutableHandleObject request,JS::MutableHandleObject promise) {
  JSAutoRealm realm(cx,stream);promise.set(JS::NewPromiseObject(cx,nullptr));
  request.set(JS_NewObjectWithGivenProto(cx,&kWriteRequestClass,nullptr));
  if (!promise || !request) return false;
  JS::SetReservedSlot(request,FPromise,JS::ObjectValue(*promise));return true;
}
void AppendWriteRequest(JSObject* stream,JSObject* request) {
  if (JSObject* tail=SlotObject(stream,WTail)) JS::SetReservedSlot(tail,FNext,JS::ObjectValue(*request));
  else JS::SetReservedSlot(stream,WHead,JS::ObjectValue(*request));
  JS::SetReservedSlot(stream,WTail,JS::ObjectValue(*request));
}
JSObject* PopWriteRequest(JSObject* stream) {
  JSObject* request=SlotObject(stream,WHead);auto next=JS::GetReservedSlot(request,FNext);
  JS::SetReservedSlot(stream,WHead,next);if (!next.isObject()) JS::SetReservedSlot(stream,WTail,JS::UndefinedValue());
  JS::SetReservedSlot(request,FNext,JS::UndefinedValue());return request;
}
CallbackResult WritableAdvance(JSContext* cx,JS::HandleObject controller,CallbackContext context) {
  JSAutoRealm realm(cx,controller);JS::RootedObject stream(cx,SlotObject(controller,KStream));
  if (!JS::GetReservedSlot(controller,KStarted).toBoolean() || WritableInFlight(stream)) return CallbackResult::Success;
  if (WritableStatus(stream)==WriteStatus::Erroring) {
    const auto result=WritableFinishErroring(cx,stream,context);
    return result==CallbackResult::Failure?CallbackOutcome(WritableNativeFailure(cx,controller)):result;
  }
  if (WritableStatus(stream)!=WriteStatus::Writable) return CallbackResult::Success;
  JS::RootedObject entry(cx,SlotObject(controller,KHead)),promise(cx),request(cx);
  if (!entry) return CallbackResult::Success;
  const bool close=JS::GetReservedSlot(entry,EClose).toBoolean();
  JS::RootedValue chunk(cx,JS::GetReservedSlot(entry,EChunk));
  if (close) {
    JS::SetReservedSlot(stream,WFlightClose,JS::GetReservedSlot(stream,WClose));
    JS::SetReservedSlot(stream,WClose,JS::UndefinedValue());PopWriteEntry(controller);
  } else {
    request=PopWriteRequest(stream);
    JS::SetReservedSlot(stream,WFlightWrite,JS::GetReservedSlot(request,FPromise));
  }
  const auto result=SinkPromise(cx,controller,close?KClose:KWrite,chunk,&promise,context);
  if (result!=CallbackResult::Success)
    return result==CallbackResult::Failure?CallbackOutcome(WritableNativeFailure(cx,controller)):result;
  if (close) ClearSinkAlgorithms(controller);
  JS::RootedObject extra(cx);
  return CallbackOutcome(ObserveSink(cx,controller,promise,close?SinkReaction::Close:SinkReaction::Write,extra) || WritableNativeFailure(cx,controller));
}
}
