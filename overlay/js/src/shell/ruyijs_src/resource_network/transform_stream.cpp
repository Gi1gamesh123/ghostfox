// This is the original Transform interface owner, shared by Window and Worker.
#include "resource_network/transform_internal.h"
#include "resource_network/transform_stream.h"
#include "resource_network/state.h"
#include "runtime/page_reference.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
const JSClass kTransformClass={"TransformStream",JSCLASS_HAS_RESERVED_SLOTS(TransformSlotCount)};
JSObject* TransformReceiver(JS::HandleValue value) {
  JSObject* object=value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr;
  return object && JS::GetClass(object)==&kTransformClass?object:nullptr;
}
namespace {
bool Get(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject stream(cx,TransformReceiver(args.thisv()));
  const unsigned slot=unsigned(WebKit::Slot(args));
  if (!stream) return Error(cx,slot==TReadable?
      "'get readable' called on an object that does not implement interface TransformStream.":
      "'get writable' called on an object that does not implement interface TransformStream.");
  args.rval().set(JS::GetReservedSlot(stream,slot));return JS_WrapValue(cx,args.rval());
}
}
bool TransformDesiredPrototype(JSContext* cx,const JS::CallArgs& args,JS::MutableHandleObject result) {
  JS::RootedObject target(cx,&args.newTarget().toObject());JS::RootedValue prototype(cx);
  if (!JS_GetProperty(cx,target,"prototype",&prototype)) return false;
  if (prototype.isObject()) {result.set(&prototype.toObject());return true;}
  JS::Realm* realm=JS::GetFunctionRealm(cx,target);if (!realm) return false;
  JS::RootedObject global(cx,JS::GetRealmGlobalOrNull(realm));
  if (!ReadPageReference(global,PageReference::ResourceTransformPrototype,&prototype) || !prototype.isObject())
    return Error(cx,"TransformStream target realm is not installed");
  result.set(&prototype.toObject());return JS_WrapObject(cx,result);
}
bool InstallTransformStreams(State& state,PageHelpers&) {
  JS::RootedObject parent(state.cx,JS::GetRealmObjectPrototype(state.cx));auto& kit=state.kit;auto& face=state.transformStream;
  if (!parent || !kit.Define("TransformStream",&kTransformClass,parent,TransformConstructor,&face) ||
      !kit.Accessor(face,"TransformStream","readable",Get,nullptr,TReadable) ||
      !kit.Accessor(face,"TransformStream","writable",Get,nullptr,TWritable) || !InstallTransformController(state)) return false;
  JS::RootedValue prototype(state.cx,JS::ObjectValue(*face.prototype));
  JS::RootedValue original(state.cx,JS::ObjectValue(*face.constructor));
  return WritePageReference(state.page,PageReference::ResourceTransformPrototype,prototype) &&
      WritePageReference(state.page,PageReference::ResourceTransformConstructor,original);
}
}
