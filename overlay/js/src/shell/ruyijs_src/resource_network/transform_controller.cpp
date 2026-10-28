#include "resource_network/transform_internal.h"
#include "resource_network/state.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
const JSClass kTransformControllerClass={"TransformStreamDefaultController",JSCLASS_HAS_RESERVED_SLOTS(TransformControllerSlotCount)};
JSObject* TransformControllerReceiver(JS::HandleValue value) {
  JSObject* object=value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr;
  return object && JS::GetClass(object)==&kTransformControllerClass?object:nullptr;
}
namespace {
bool Illegal(JSContext* cx,unsigned,JS::Value*) {return Error(cx,"Illegal constructor.");}
bool Desired(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject controller(cx,TransformControllerReceiver(args.thisv()));
  if (!controller) return Error(cx,"'get desiredSize' called on an object that does not implement interface TransformStreamDefaultController.");
  JS::RootedObject stream(cx,SlotObject(controller,CTransform)),readable(cx,SlotObject(stream,TReadable));
  JS::RootedObject source(cx,SlotObject(readable,SController));
  const auto status=ReadableStatus(readable);
  if (status==StreamStatus::Errored) args.rval().setNull();
  else args.rval().setDouble(status==StreamStatus::Closed?0:DefaultDesiredSize(source));
  return true;
}
bool Enqueue(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject controller(cx,TransformControllerReceiver(args.thisv()));
  if (!controller) return Error(cx,"'enqueue' called on an object that does not implement interface TransformStreamDefaultController.");
  args.rval().setUndefined();return TransformEnqueue(cx,controller,args.get(0));
}
bool Fail(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject controller(cx,TransformControllerReceiver(args.thisv()));
  if (!controller) return Error(cx,"'error' called on an object that does not implement interface TransformStreamDefaultController.");
  JS::RootedObject stream(cx,SlotObject(controller,CTransform));
  args.rval().setUndefined();
  return TransformError(cx,stream,args.get(0)) || TransformNativeFailure(cx,stream);
}
bool Terminate(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject controller(cx,TransformControllerReceiver(args.thisv()));
  if (!controller) return Error(cx,"'terminate' called on an object that does not implement interface TransformStreamDefaultController.");
  args.rval().setUndefined();return TransformTerminate(cx,controller);
}
}
bool InstallTransformController(State& state) {
  JS::RootedObject parent(state.cx,JS::GetRealmObjectPrototype(state.cx));
  auto& kit=state.kit;auto& face=state.transformController;
  return parent && kit.Define("TransformStreamDefaultController",&kTransformControllerClass,parent,Illegal,&face) &&
      kit.Method(face,"TransformStreamDefaultController","enqueue",Enqueue,0,0) &&
      kit.Method(face,"TransformStreamDefaultController","error",Fail,0,0) &&
      kit.Method(face,"TransformStreamDefaultController","terminate",Terminate,0,0) &&
      kit.Accessor(face,"TransformStreamDefaultController","desiredSize",Desired,nullptr,0);
}
}
