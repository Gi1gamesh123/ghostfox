// Realm-local Streams factories; the original installer remains the only owner.
#include "resource_network/readable_stream.h"
#include "resource_network/readable_controller.h"
#include "resource_network/readable_byte.h"
#include "resource_network/readable_tee.h"
#include "jsfriendapi.h"
#include "js/GlobalObject.h"
#include "resource_network/state.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_reference.h"
#include "js/PropertyAndElement.h"
#include "js/Promise.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::resource_network {
namespace {
const JSClass kStreamPrototypes={"Resource stream prototypes",JSCLASS_HAS_RESERVED_SLOTS(SFSlots)};
}
bool StreamFactories(JSContext* cx,JS::HandleObject source,JS::MutableHandleObject result) {
  JS::RootedObject global(cx,JS::GetNonCCWObjectGlobal(source));
  JS::RootedValue retained(cx);
  if (!ReadPageReference(global,PageReference::ResourceStreamPrototypes,&retained) ||
      !retained.isObject() || JS::GetClass(&retained.toObject())!=&kStreamPrototypes)
    return Error(cx,"ReadableStream source realm is not installed");
  result.set(&retained.toObject());return true;
}
bool StreamDesiredPrototype(JSContext* cx,const JS::CallArgs& args,unsigned kind,
                      JS::MutableHandleObject result) {
  JS::RootedObject target(cx,&args.newTarget().toObject());
  JS::RootedValue prototype(cx);
  if (!JS_GetProperty(cx,target,"prototype",&prototype)) return false;
  if (prototype.isObject()) {result.set(&prototype.toObject());return true;}
  JS::Realm* realm=JS::GetFunctionRealm(cx,target);
  if (!realm) return false;
  JS::RootedObject global(cx,JS::GetRealmGlobalOrNull(realm));
  JS::RootedValue retained(cx);
  if (!ReadPageReference(global,PageReference::ResourceStreamPrototypes,&retained) ||
      !retained.isObject() || JS::GetClass(&retained.toObject())!=&kStreamPrototypes)
    return Error(cx,"ReadableStream target realm is not installed");
  result.set(SlotObject(&retained.toObject(),kind));
  return JS_WrapObject(cx,result);
}
const JSClass kReadableStreamClass={"ReadableStream",JSCLASS_HAS_RESERVED_SLOTS(SSlots)};
const JSClass kReadableReaderClass={"ReadableStreamDefaultReader",JSCLASS_HAS_RESERVED_SLOTS(RSlots)};
const JSClass kReadRequestClass={"Resource read request",JSCLASS_HAS_RESERVED_SLOTS(QSlots)};
JSObject* StreamReceiver(JS::HandleValue value) {
  JSObject* object=value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr;
  return object && JS::GetClass(object)==&kReadableStreamClass?object:nullptr;
}
JSObject* ReaderReceiver(JS::HandleValue value) {
  JSObject* object=value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr;
  return object && JS::GetClass(object)==&kReadableReaderClass?object:nullptr;
}
BodyRecord* StreamBody(JSObject* stream) { return HolderBody(SlotObject(stream,SHolder)); }
bool NewReadableStream(JSContext* cx,JS::HandleObject prototype,JS::HandleObject readerPrototype,
                       JS::HandleObject holder,JS::MutableHandleObject result) {
  result.set(JS_NewObjectWithGivenProto(cx,&kReadableStreamClass,prototype));
  if (!result) return false;
  JS::SetReservedSlot(result,SHolder,JS::ObjectOrNullValue(holder));
  JS::SetReservedSlot(result,SReaderPrototype,JS::ObjectValue(*readerPrototype));
  JS::SetReservedSlot(result,SSource,JS::Int32Value(int(holder?StreamSource::BufferedBody:StreamSource::Default)));
  JS::SetReservedSlot(result,SDisturbed,JS::BooleanValue(false));
  SetReadableStatus(result,StreamStatus::Readable);
  SetFlags(result,0);
  return true;
}
bool NewDefaultReader(JSContext* cx,JS::HandleObject stream,JS::HandleObject prototype,
                      JS::MutableHandleObject result) {
  return NewReader(cx,stream,prototype,false,result);
}
bool NewReader(JSContext* cx,JS::HandleObject stream,JS::HandleObject prototype,
                bool byob,JS::MutableHandleObject result) {
  if (!JS::GetReservedSlot(stream,SReader).isUndefined()) return Error(cx,"ReadableStream is locked");
  if (byob && !IsByteStream(stream)) return Error(cx,"Trying to read with incompatible controller");
  JS::RootedObject reader(cx,JS_NewObjectWithGivenProto(cx,byob?&kByobReaderClass:&kReadableReaderClass,prototype));
  JS::RootedObject closed(cx,JS::NewPromiseObject(cx,nullptr));
  JS::RootedObject wrappedStream(cx,stream),wrappedReader(cx,reader);
  if (!reader || !closed) return false;
  if (ReadableStatus(stream)==StreamStatus::Closed && !JS::ResolvePromise(cx,closed,JS::UndefinedHandleValue)) return false;
  if (ReadableStatus(stream)==StreamStatus::Errored) {
    JS::RootedValue reason(cx,JS::GetReservedSlot(stream,SError));
    if (!JS_WrapValue(cx,&reason) || !JS::RejectPromise(cx,closed,reason) ||
        !JS::SetAnyPromiseIsHandled(cx,closed)) return false;
  }
  if (!JS_WrapObject(cx,&wrappedStream)) return false;
  {
    JSAutoRealm realm(cx,stream);
    if (!JS_WrapObject(cx,&wrappedReader)) return false;
  }
  JS::SetReservedSlot(reader,RStream,JS::ObjectValue(*wrappedStream));
  JS::SetReservedSlot(reader,RClosed,JS::ObjectValue(*closed));
  JS::SetReservedSlot(stream,SReader,JS::ObjectValue(*wrappedReader));
  result.set(reader);
  return true;
}
bool StreamConstructor(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  NativeConstructorTrace trace(cx,args);
  State& state=*FunctionOwner(args);
  if (!args.isConstructing()) return Error(cx,"ReadableStream requires new");
  JS::RootedObject proto(cx),holder(cx),object(cx),readerProto(cx,state.defaultReader.prototype);
  if (!StreamDesiredPrototype(cx,args,0,&proto) ||
      !NewReadableStream(cx,proto,readerProto,holder,&object)) return false;
  JS::SetReservedSlot(object,SByobPrototype,JS::ObjectValue(*state.byobReader.prototype));
  if (!SetupDefaultReadable(state,object,args.get(0),args.get(1))) return false;
  args.rval().setObject(*object);
  return trace.Finish(true);
}
State* StreamFactoryState(JSContext* cx,JS::HandleObject factories) {
  JS::RootedObject record(cx,js::CheckedUnwrapStatic(factories));
  if (!record || JS::GetClass(record)!=&kStreamPrototypes) {
    Error(cx,"ReadableStream factory record is invalid");return nullptr;
  }
  JS::RootedObject original(cx,SlotObject(record,SFOwner));
  if (!original ||
      !JS_IsNativeFunction(original,StreamConstructor)) {
    Error(cx,"ReadableStream factory owner is unavailable");return nullptr;
  }
  auto owner=js::GetFunctionNativeReserved(original,0);
  auto kind=js::GetFunctionNativeReserved(original,1);
  if (!owner.isDouble() || !kind.isInt32() || kind.toInt32()!=0 || !owner.toPrivate()) {
    Error(cx,"ReadableStream factory owner is invalid");return nullptr;
  }
  auto* state=static_cast<State*>(owner.toPrivate());
  if (state->cx!=cx || state->stream.constructor!=original) {
    Error(cx,"ReadableStream factory identity changed");return nullptr;
  }
  return state;
}
bool ReaderConstructor(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  NativeConstructorTrace trace(cx,args);
  if (!args.isConstructing()) return Error(cx,"ReadableStreamDefaultReader requires new");
  JS::RootedObject proto(cx),stream(cx,StreamReceiver(args.get(0))),reader(cx);
  if (!StreamDesiredPrototype(cx,args,1,&proto)) return false;
  if (!stream) return Error(cx,"ReadableStreamDefaultReader requires a stream");
  if (!NewDefaultReader(cx,stream,proto,&reader)) return false;
  args.rval().setObject(*reader);
  return trace.Finish(true);
}
bool InstallReadableStream(State& state,PageHelpers&) {
  JS::RootedObject parent(state.cx,JS::GetRealmObjectPrototype(state.cx));
  JS::RootedObject retained(state.cx,JS_NewObjectWithGivenProto(state.cx,&kStreamPrototypes,nullptr));
  auto& k=state.kit;
  if (!(parent && retained && k.Define("ReadableStream",&kReadableStreamClass,parent,StreamConstructor,&state.stream) &&
      k.Define("ReadableStreamDefaultReader",&kReadableReaderClass,parent,ReaderConstructor,&state.defaultReader,1) &&
      k.Accessor(state.stream,"ReadableStream","locked",StreamLocked,nullptr,0) &&
      k.Method(state.stream,"ReadableStream","cancel",StreamCancel,0,0) &&
      k.Method(state.stream,"ReadableStream","getReader",StreamGetReader,0,0) &&
      k.Method(state.stream,"ReadableStream","pipeTo",StreamPipeTo,1,0) &&
      k.Method(state.stream,"ReadableStream","tee",StreamTee,0,0) &&
      k.Method(state.defaultReader,"ReadableStreamDefaultReader","read",ReaderRead,0,0) &&
      k.Method(state.defaultReader,"ReadableStreamDefaultReader","releaseLock",ReaderRelease,0,0) &&
      k.Method(state.defaultReader,"ReadableStreamDefaultReader","cancel",ReaderCancel,0,0) &&
      k.Accessor(state.defaultReader,"ReadableStreamDefaultReader","closed",ReaderClosed,nullptr,0) &&
      InstallDefaultController(state) && InstallByteInterfaces(state))) return false;
  JS::SetReservedSlot(retained,0,JS::ObjectValue(*state.stream.prototype));
  JS::SetReservedSlot(retained,1,JS::ObjectValue(*state.defaultReader.prototype));
  JS::SetReservedSlot(retained,2,JS::ObjectValue(*state.byobReader.prototype));
  JS::SetReservedSlot(retained,SFDefaultController,JS::ObjectValue(*state.defaultController.prototype));
  JS::SetReservedSlot(retained,SFByteController,JS::ObjectValue(*state.byteController.prototype));
  JS::SetReservedSlot(retained,SFByobRequest,JS::ObjectValue(*state.byobRequest.prototype));
  JS::SetReservedSlot(retained,SFOwner,JS::ObjectValue(*state.stream.constructor));
  JS::RootedValue value(state.cx,JS::ObjectValue(*retained));
  return WritePageReference(state.page,PageReference::ResourceStreamPrototypes,value);
}
}
