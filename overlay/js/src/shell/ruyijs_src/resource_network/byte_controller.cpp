// The byte controller is installed only through the existing Streams owner.
#include "resource_network/readable_byte.h"
#include "resource_network/state.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
JSObject* ByteControllerReceiver(JS::HandleValue value) {
  JSObject* object=value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr;
  return object && JS::GetClass(object)==&kByteControllerClass?object:nullptr;
}
namespace {
bool Construct(JSContext* cx,unsigned,JS::Value*) { return Error(cx,"Illegal constructor."); }
bool Desired(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject controller(cx,ByteControllerReceiver(args.thisv()));
  if (!controller) return Error(cx,"'get desiredSize' called on an object that does not implement interface ReadableByteStreamController.");
  auto status=ReadableStatus(SlotObject(controller,DStream));
  if (status==StreamStatus::Errored) args.rval().setNull();
  else args.rval().setDouble(status==StreamStatus::Closed?0:ByteDesiredSize(controller));
  return true;
}
bool Request(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject controller(cx,ByteControllerReceiver(args.thisv())),request(cx);
  if (!controller) return Error(cx,"'get byobRequest' called on an object that does not implement interface ReadableByteStreamController.");
  if (!GetByobRequest(cx,controller,&request)) return false;
  args.rval().setObjectOrNull(request);return JS_WrapValue(cx,args.rval());
}
bool Close(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject controller(cx,ByteControllerReceiver(args.thisv()));
  if (!controller) return Error(cx,"'close' called on an object that does not implement interface ReadableByteStreamController.");
  if (!ByteClose(cx,controller)) return false;
  args.rval().setUndefined();return true;
}
bool Enqueue(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject controller(cx,ByteControllerReceiver(args.thisv()));
  if (!controller) return Error(cx,"'enqueue' called on an object that does not implement interface ReadableByteStreamController.");
  if (!argc) return Error(cx,"ReadableByteStreamController.enqueue: At least 1 argument required, but only 0 passed");
  if (!ByteEnqueue(cx,controller,args.get(0))) return false;
  args.rval().setUndefined();return true;
}
bool Fail(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject controller(cx,ByteControllerReceiver(args.thisv()));
  if (!controller) return Error(cx,"'error' called on an object that does not implement interface ReadableByteStreamController.");
  if (!ByteError(cx,controller,args.get(0))) return false;
  args.rval().setUndefined();return true;
}
}
bool InstallByteInterfaces(State& state) {
  JS::RootedObject parent(state.cx,JS::GetRealmObjectPrototype(state.cx));
  auto& kit=state.kit;auto& face=state.byteController;
  return parent && kit.Define("ReadableByteStreamController",&kByteControllerClass,parent,Construct,&face) &&
      kit.Method(face,"ReadableByteStreamController","close",Close,0,0) &&
      kit.Method(face,"ReadableByteStreamController","enqueue",Enqueue,1,0) &&
      kit.Method(face,"ReadableByteStreamController","error",Fail,0,0) &&
      kit.Accessor(face,"ReadableByteStreamController","byobRequest",Request,nullptr,0) &&
      kit.Accessor(face,"ReadableByteStreamController","desiredSize",Desired,nullptr,0) &&
      InstallByobReader(state) && InstallByobRequest(state);
}
}
