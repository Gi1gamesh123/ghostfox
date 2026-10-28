// Native consumers compose the sole original stream factories/controllers.
#include "resource_network/native_endpoints.h"
#include "resource_network/readable_byte.h"
#include "resource_network/writable_stream.h"
#include "resource_network/state.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
bool NewNativeReadableStream(State& state,JS::HandleObject operation,const NativeSourceSteps& steps,
    bool bytes,double highWaterMark,JS::HandleObject prototype,
    JS::MutableHandleObject stream,JS::MutableHandleObject controller) {
  JSContext* cx=state.cx;bool initialized=false;
  {
    JSAutoRealm realm(cx,state.page);
    JS::RootedObject proto(cx,prototype?prototype.get():state.stream.prototype.get());
    JS::RootedObject reader(cx,state.defaultReader.prototype),holder(cx),algorithms(cx);
    if (!JS_WrapObject(cx,&proto) || !NewReadableStream(cx,proto,reader,holder,stream) ||
        !NewNativeSourceAlgorithms(cx,operation,steps,&algorithms)) return false;
    JS::RootedObject local(cx,operation);if (!JS_WrapObject(cx,&local)) return false;
    JS::SetReservedSlot(stream,SNativeOwner,JS::ObjectValue(*local));
    JS::SetReservedSlot(stream,SByobPrototype,JS::ObjectValue(*state.byobReader.prototype));
    JS::RootedValue source(cx,JS::ObjectValue(*algorithms));
    initialized=InitializeReadableController(state,stream,source,JS::UndefinedHandleValue,
        JS::UndefinedHandleValue,JS::UndefinedHandleValue,JS::UndefinedHandleValue,highWaterMark,bytes,0);
    controller.set(SlotObject(stream,SController));
  }
  if (initialized && JS_WrapObject(cx,stream) && JS_WrapObject(cx,controller)) return true;
  if (controller) ClearDefaultAlgorithms(js::UncheckedUnwrap(controller));
  return false;
}
bool NewNativeWritableStream(State& state,JS::HandleObject operation,const NativeSinkSteps& steps,
    double highWaterMark,JS::HandleObject prototype,
    JS::MutableHandleObject stream,JS::MutableHandleObject controller) {
  JSContext* cx=state.cx;bool initialized=false;
  {
    JSAutoRealm realm(cx,state.page);
    JS::RootedObject proto(cx,prototype?prototype.get():state.writableStream.prototype.get()),algorithms(cx);
    if (!JS_WrapObject(cx,&proto) || !NewWritableStream(state,proto,stream) ||
        !NewNativeSinkAlgorithms(cx,operation,steps,&algorithms)) return false;
    JS::RootedObject local(cx,operation);if (!JS_WrapObject(cx,&local)) return false;
    JS::SetReservedSlot(stream,WNativeOwner,JS::ObjectValue(*local));
    JS::RootedValue sink(cx,JS::ObjectValue(*algorithms));
    initialized=InitializeWritableController(state,stream,sink,JS::UndefinedHandleValue,
        JS::UndefinedHandleValue,JS::UndefinedHandleValue,JS::UndefinedHandleValue,
        JS::UndefinedHandleValue,highWaterMark);
    controller.set(SlotObject(stream,WController));
  }
  if (initialized && JS_WrapObject(cx,stream) && JS_WrapObject(cx,controller)) return true;
  if (controller) ClearSinkAlgorithms(js::UncheckedUnwrap(controller));
  return false;
}
bool NativeReadableEnqueue(JSContext* cx,JS::HandleObject input,JS::HandleValue chunk) {
  JS::RootedObject controller(cx,js::CheckedUnwrapStatic(input));if (!controller) return false;
  JSAutoRealm realm(cx,controller);JS::RootedValue local(cx,chunk);
  if (!JS_WrapValue(cx,&local)) return false;
  return JS::GetClass(controller)==&kByteControllerClass?ByteEnqueue(cx,controller,local):DefaultEnqueue(cx,controller,local);
}
bool NativeReadableClose(JSContext* cx,JS::HandleObject input) {
  JS::RootedObject controller(cx,js::CheckedUnwrapStatic(input));if (!controller) return false;
  JSAutoRealm realm(cx,controller);
  if(JS::GetClass(controller)!=&kByteControllerClass)return DefaultClose(cx,controller);
  if(!ByteClose(cx,controller,ByteCloseKind::Intrinsic))return false;
  JS::RootedObject stream(cx,SlotObject(controller,DStream));
  return ReadableStatus(stream)!=StreamStatus::Closed || !SlotObject(controller,BPendingHead) || ByteRespond(cx,controller,0);
}
bool NativeReadableError(JSContext* cx,JS::HandleObject input,JS::HandleValue reason) {
  JS::RootedObject controller(cx,js::CheckedUnwrapStatic(input));if (!controller) return false;
  JSAutoRealm realm(cx,controller);JS::RootedValue local(cx,reason);
  if (!JS_WrapValue(cx,&local)) return false;
  return JS::GetClass(controller)==&kByteControllerClass?ByteError(cx,controller,local):DefaultError(cx,controller,local);
}
bool NativeWritableError(JSContext* cx,JS::HandleObject input,JS::HandleValue reason) {
  JS::RootedObject controller(cx,js::CheckedUnwrapStatic(input));if (!controller) return false;
  JSAutoRealm realm(cx,controller);JS::RootedValue local(cx,reason);
  return JS_WrapValue(cx,&local) && WritableControllerError(cx,controller,local);
}
}
