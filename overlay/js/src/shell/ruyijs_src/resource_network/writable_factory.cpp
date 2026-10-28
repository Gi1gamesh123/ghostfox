// The original ResourceNetwork installer supplies all three realm-local faces.
#include "resource_network/writable_stream.h"
#include "resource_network/state.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_reference.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
namespace {const JSClass kPrototypes={"Resource writable prototypes",JSCLASS_HAS_RESERVED_SLOTS(2)};}
bool WritableDesiredPrototype(JSContext* cx,const JS::CallArgs& args,unsigned kind,JS::MutableHandleObject result) {
  JS::RootedObject target(cx,&args.newTarget().toObject());JS::RootedValue prototype(cx);
  if (!JS_GetProperty(cx,target,"prototype",&prototype)) return false;
  if (prototype.isObject()) {result.set(&prototype.toObject());return true;}
  JS::Realm* realm=JS::GetFunctionRealm(cx,target);if (!realm) return false;
  JS::RootedObject global(cx,JS::GetRealmGlobalOrNull(realm));JS::RootedValue retained(cx);
  if (!ReadPageReference(global,PageReference::ResourceWritablePrototypes,&retained) ||
      !retained.isObject() || JS::GetClass(&retained.toObject())!=&kPrototypes)
    return Error(cx,"WritableStream target realm is not installed");
  result.set(SlotObject(&retained.toObject(),kind));return JS_WrapObject(cx,result);
}
bool NewWritableStream(State& state,JS::HandleObject prototype,JS::MutableHandleObject result) {
  JSContext* cx=state.cx;
  JS::RootedObject stream(cx,JS_NewObjectWithGivenProto(cx,&kWritableClass,prototype));if (!stream) return false;
  SetWritableStatus(stream,WriteStatus::Writable);
  JS::SetReservedSlot(stream,WWriterPrototype,JS::ObjectValue(*state.writableWriter.prototype));
  result.set(stream);return true;
}
bool WritableConstructor(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);NativeConstructorTrace trace(cx,args);
  if (!args.isConstructing()) return Error(cx,"WritableStream constructor: 'new' is required");
  auto& state=*FunctionOwner(args);JS::RootedObject prototype(cx);
  if (!WritableDesiredPrototype(cx,args,0,&prototype)) return false;
  JS::RootedObject stream(cx);
  if (!NewWritableStream(state,prototype,&stream) || !SetupWritable(state,stream,args.get(0),args.get(1))) return false;
  args.rval().setObject(*stream);return trace.Finish(true);
}
bool InstallWritableStream(State& state,PageHelpers&) {
  JS::RootedObject parent(state.cx,JS::GetRealmObjectPrototype(state.cx));
  JS::RootedObject retained(state.cx,JS_NewObjectWithGivenProto(state.cx,&kPrototypes,nullptr));auto& k=state.kit;auto& face=state.writableStream;
  if (!(parent && retained && k.Define("WritableStream",&kWritableClass,parent,WritableConstructor,&face) &&
      k.Method(face,"WritableStream","abort",WritableAbortCall,0,0) &&
      k.Method(face,"WritableStream","close",WritableCloseCall,0,0) &&
      k.Method(face,"WritableStream","getWriter",WritableGetWriter,0,0) &&
      k.Accessor(face,"WritableStream","locked",WritableLocked,nullptr,0) &&
      InstallWritableWriter(state) && InstallSinkController(state))) return false;
  JS::SetReservedSlot(retained,0,JS::ObjectValue(*face.prototype));
  JS::SetReservedSlot(retained,1,JS::ObjectValue(*state.writableWriter.prototype));
  JS::RootedValue value(state.cx,JS::ObjectValue(*retained));
  return WritePageReference(state.page,PageReference::ResourceWritablePrototypes,value);
}
}
