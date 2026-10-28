// Shared traced storage and Promise settlement; no persistent instance roots.
#include "resource_network/writable_stream.h"
#include "resource_network/state.h"
#include "js/Promise.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
const JSClass kWritableClass={"WritableStream",JSCLASS_HAS_RESERVED_SLOTS(WSlots)};
const JSClass kWriterClass={"WritableStreamDefaultWriter",JSCLASS_HAS_RESERVED_SLOTS(VSlots)};
const JSClass kSinkControllerClass={"WritableStreamDefaultController",JSCLASS_HAS_RESERVED_SLOTS(KSlots)};
const JSClass kWriteEntryClass={"Resource writable entry",JSCLASS_HAS_RESERVED_SLOTS(ESlots)};
const JSClass kWriteRequestClass={"Resource write request",JSCLASS_HAS_RESERVED_SLOTS(FSlots)};
namespace {
JSObject* Receiver(JS::HandleValue value,const JSClass* clasp) {
  JSObject* object=value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr;
  return object && JS::GetClass(object)==clasp?object:nullptr;
}
}
JSObject* WritableReceiver(JS::HandleValue v) {return Receiver(v,&kWritableClass);}
JSObject* WriterReceiver(JS::HandleValue v) {return Receiver(v,&kWriterClass);}
JSObject* SinkReceiver(JS::HandleValue v) {return Receiver(v,&kSinkControllerClass);}
bool WritableStore(JSContext* cx,JS::HandleObject target,unsigned slot,JS::HandleValue value) {
  JSAutoRealm realm(cx,target);JS::RootedValue wrapped(cx,value);
  if (!JS_WrapValue(cx,&wrapped)) return false;
  JS::SetReservedSlot(target,slot,wrapped);return true;
}
bool WritableStoreObject(JSContext* cx,JS::HandleObject target,unsigned slot,JS::HandleObject object) {
  JS::RootedValue value(cx,JS::ObjectOrNullValue(object));return WritableStore(cx,target,slot,value);
}
bool WritablePromise(JSContext* cx,JS::HandleObject promise,bool reject,JS::HandleValue reason,bool handled) {
  JSAutoRealm realm(cx,promise);JS::RootedValue value(cx,reason);
  if (!JS_WrapValue(cx,&value)) return false;
  return (reject?JS::RejectPromise(cx,promise,value):JS::ResolvePromise(cx,promise,value)) &&
      (!handled || JS::SetAnyPromiseIsHandled(cx,promise));
}
bool WriterReject(JSContext* cx,JS::HandleObject writer,unsigned slot,JS::HandleValue reason) {
  JSAutoRealm realm(cx,writer);JS::RootedObject promise(cx,SlotObject(writer,slot));
  if (JS::GetPromiseState(promise)!=JS::PromiseState::Pending) {
    promise=JS::NewPromiseObject(cx,nullptr);
    if (!promise) return false;
    JS::SetReservedSlot(writer,slot,JS::ObjectValue(*promise));
  }
  return WritablePromise(cx,promise,true,reason,true);
}
bool WritableTypeRejection(JSContext* cx,JS::HandleObject owner,const char* message,JS::MutableHandleValue result) {
  {JSAutoRealm realm(cx,owner);if (!TypeErrorPromise(cx,message,result)) return false;}
  return JS_WrapValue(cx,result);
}
bool WritableExceptionRejection(JSContext* cx,JS::MutableHandleValue result) {
  JS::RootedValue error(cx);
  if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx,&error)) return false;
  JS_ClearPendingException(cx);return JS_WrapValue(cx,&error) && Rejected(cx,error,result);
}
void ClearSinkAlgorithms(JSObject* controller) {
  for (unsigned slot=KSink;slot<=KSize;++slot) JS::SetReservedSlot(controller,slot,JS::UndefinedValue());
}
void ResetWritableQueue(JSObject* controller) {
  JS::SetReservedSlot(controller,KHead,JS::UndefinedValue());
  JS::SetReservedSlot(controller,KTail,JS::UndefinedValue());
  JS::SetReservedSlot(controller,KTotal,JS::DoubleValue(0));
}
double WritableDesiredSize(JSObject* controller) {
  return JS::GetReservedSlot(controller,KHwm).toNumber()-JS::GetReservedSlot(controller,KTotal).toNumber();
}
bool WritablePressure(JSContext* cx,JS::HandleObject stream,bool pressure) {
  JS::RootedObject writer(cx,SlotObject(stream,WWriter));
  if (writer && JS::GetReservedSlot(stream,WPressure).toBoolean()!=pressure) {
    JSAutoRealm realm(cx,writer);JS::RootedObject ready(cx,SlotObject(writer,VReady));
    if (pressure) {
      ready=JS::NewPromiseObject(cx,nullptr);if (!ready) return false;
      JS::SetReservedSlot(writer,VReady,JS::ObjectValue(*ready));
    } else if (!WritablePromise(cx,ready,false,JS::UndefinedHandleValue)) return false;
  }
  JS::SetReservedSlot(stream,WPressure,JS::BooleanValue(pressure));return true;
}
}
