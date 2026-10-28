// BYOB owns its brand and view conversion; reader lock/status remain shared.
#include "resource_network/readable_byte.h"
#include "resource_network/state.h"
#include "runtime/native_constructor_trace.h"
#include "js/PropertyAndElement.h"
#include "js/Promise.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
const JSClass kByobReaderClass={"ReadableStreamBYOBReader",JSCLASS_HAS_RESERVED_SLOTS(RSlots)};
bool IsByobReader(JSObject* reader) { return reader && JS::GetClass(reader)==&kByobReaderClass; }
JSObject* ByobReaderReceiver(JS::HandleValue value) {
  JSObject* object=value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr;
  return IsByobReader(object)?object:nullptr;
}
bool NewByobReader(JSContext* cx,JS::HandleObject stream,JS::HandleObject prototype,JS::MutableHandleObject result) {
  return NewReader(cx,stream,prototype,true,result);
}
namespace {
bool Construct(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);NativeConstructorTrace trace(cx,args);
  if (!args.isConstructing()) return Error(cx,"ReadableStreamBYOBReader requires new");
  JS::RootedObject stream(cx,StreamReceiver(args.get(0))),prototype(cx),reader(cx);
  if (!StreamDesiredPrototype(cx,args,2,&prototype)) return false;
  if (!stream) return Error(cx,"ReadableStreamBYOBReader requires a stream");
  if (!NewByobReader(cx,stream,prototype,&reader)) return false;
  args.rval().setObject(*reader);return trace.Finish(true);
}
bool ReadImpl(JSContext* cx,const JS::CallArgs& args) {
  JS::RootedObject reader(cx,ByobReaderReceiver(args.thisv())),buffer(cx);
  if (!reader) return Error(cx,"'read' called on an object that does not implement interface ReadableStreamBYOBReader.");
  if (!args.length()) return Error(cx,"ReadableStreamBYOBReader.read: At least 1 argument required, but only 0 passed");
  ByteView info;
  if (!ConvertByteView(cx,args.get(0),&info,&buffer,"ReadableStreamBYOBReader.read")) return false;
  uint64_t minimum=1;
  if (!args.get(1).isNullOrUndefined()) {
    if (!args[1].isObject()) return Error(cx,"ReadableStreamBYOBReader.read options must be a dictionary");
    JS::RootedObject options(cx,&args[1].toObject());JS::RootedValue value(cx);
    if (!JS_GetProperty(cx,options,"min",&value)) return false;
    if (!value.isUndefined() && !EnforceU64(cx,value,&minimum,
        "ReadableStreamBYOBReader.read: 'min' member of ReadableStreamBYOBReaderReadOptions")) return false;
  }
  // Reentrant min conversion can detach the original input; re-read native slots.
  if (!ConvertByteView(cx,args.get(0),&info,&buffer,"ReadableStreamBYOBReader.read")) return false;
  if (!info.length) return Error(cx,"ReadableStreamBYOBReader.read: Zero Length View");
  if (!info.bufferLength) return Error(cx,"ReadableStreamBYOBReader.read: zero length viewed buffer");
  if (!minimum) return Error(cx,"ReadableStreamBYOBReader.read: Zero is not a valid value for 'min' member of ReadableStreamBYOBReaderReadOptions.");
  if (minimum>info.length/info.element) return Error(cx,info.type==JS::Scalar::MaxTypedArrayViewType ?
      "ReadableStreamBYOBReader.read: byteLength exceeded by 'min' member of ReadableStreamBYOBReaderReadOptions.":
      "ReadableStreamBYOBReader.read: Array length exceeded by 'min' member of ReadableStreamBYOBReaderReadOptions.",true);
  JS::RootedObject stream(cx,SlotObject(reader,RStream));
  if (!stream) return Error(cx,"ReadableStreamBYOBReader.read: Reader has undefined stream");
  {
    JSAutoRealm realm(cx,reader);
    JS::RootedObject promise(cx,JS::NewPromiseObject(cx,nullptr));
    if (!promise) return false;
    DisturbReadable(stream);
    if (ReadableStatus(stream)==StreamStatus::Errored) {
      JS::RootedValue reason(cx,JS::GetReservedSlot(stream,SError));
      if (!JS_WrapValue(cx,&reason) || !JS::RejectPromise(cx,promise,reason)) return false;
    } else if (!ByteRead(cx,stream,promise,buffer,info,minimum) ||
               !ScheduleEnd(*FunctionOwner(args),stream)) return false;
    args.rval().setObject(*promise);
  }
  return JS_WrapValue(cx,args.rval());
}
bool Read(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  return ReadImpl(cx,args) || RejectPendingException(cx,args.rval());
}
bool Closed(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject reader(cx,ByobReaderReceiver(args.thisv()));
  if (!reader) return TypeErrorPromise(cx,"'get closed' called on an object that does not implement interface ReadableStreamBYOBReader.",args.rval());
  args.rval().set(JS::GetReservedSlot(reader,RClosed));return JS_WrapValue(cx,args.rval());
}
bool Cancel(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject reader(cx,ByobReaderReceiver(args.thisv()));
  if (!reader) return TypeErrorPromise(cx,"'cancel' called on an object that does not implement interface ReadableStreamBYOBReader.",args.rval());
  return CancelReader(cx,reader,args.get(0),args.rval());
}
bool Release(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject reader(cx,ByobReaderReceiver(args.thisv()));
  if (!reader) return Error(cx,"'releaseLock' called on an object that does not implement interface ReadableStreamBYOBReader.");
  args.rval().setUndefined();return ReleaseReader(cx,reader);
}
}
bool InstallByobReader(State& state) {
  JS::RootedObject parent(state.cx,JS::GetRealmObjectPrototype(state.cx));
  auto& kit=state.kit;auto& face=state.byobReader;
  return parent && kit.Define("ReadableStreamBYOBReader",&kByobReaderClass,parent,Construct,&face,1) &&
      kit.Method(face,"ReadableStreamBYOBReader","read",Read,1,0) &&
      kit.Method(face,"ReadableStreamBYOBReader","releaseLock",Release,0,0) &&
      kit.Method(face,"ReadableStreamBYOBReader","cancel",Cancel,0,0) &&
      kit.Accessor(face,"ReadableStreamBYOBReader","closed",Closed,nullptr,0);
}
}
