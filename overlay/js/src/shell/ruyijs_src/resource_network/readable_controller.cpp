// Default-source controller values use this original ResourceNetwork factory.
#include "resource_network/readable_controller.h"
#include "resource_network/state.h"
#include "js/GlobalObject.h"
#include "js/Realm.h"
#include <initializer_list>

namespace ruyijs::resource_network {
const JSClass kDefaultControllerClass={"ReadableStreamDefaultController",JSCLASS_HAS_RESERVED_SLOTS(DSlots)};
const JSClass kReadChunkClass={"Resource stream chunk",JSCLASS_HAS_RESERVED_SLOTS(ReadChunkSlots)};
JSObject* DefaultControllerReceiver(JS::HandleValue value) {
  JSObject* object=value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr;
  return object && JS::GetClass(object)==&kDefaultControllerClass?object:nullptr;
}
double DefaultDesiredSize(JSObject* controller) {
  return JS::GetReservedSlot(controller,DHwm).toNumber()-
         JS::GetReservedSlot(controller,DTotal).toNumber();
}
void ResetDefaultQueue(JSObject* controller) {
  JS::SetReservedSlot(controller,DHead,JS::UndefinedValue());
  JS::SetReservedSlot(controller,DTail,JS::UndefinedValue());
  JS::SetReservedSlot(controller,DTotal,JS::DoubleValue(0));
}
void ClearDefaultAlgorithms(JSObject* controller) {
  for (unsigned slot : {DSource,DStart,DPull,DCancel,DSize})
    JS::SetReservedSlot(controller,slot,JS::UndefinedValue());
}
namespace {
bool Construct(JSContext* cx,unsigned,JS::Value*) { return Error(cx,"Illegal constructor."); }
bool Desired(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject controller(cx,DefaultControllerReceiver(args.thisv()));
  if (!controller) return Error(cx,"desiredSize requires a ReadableStreamDefaultController");
  StreamStatus status=ReadableStatus(SlotObject(controller,DStream));
  if (status==StreamStatus::Errored) args.rval().setNull();
  else args.rval().setDouble(status==StreamStatus::Closed?0:DefaultDesiredSize(controller));
  return true;
}
bool Close(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject controller(cx,DefaultControllerReceiver(args.thisv()));
  if (!controller) return Error(cx,"close requires a ReadableStreamDefaultController");
  if (!DefaultClose(cx,controller)) return false;
  args.rval().setUndefined();return true;
}
bool Enqueue(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject controller(cx,DefaultControllerReceiver(args.thisv()));
  if (!controller) return Error(cx,"enqueue requires a ReadableStreamDefaultController");
  JS::RootedValue value(cx,args.get(0));
  if (!DefaultEnqueue(cx,controller,value)) return false;
  args.rval().setUndefined();return true;
}
bool Fail(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject controller(cx,DefaultControllerReceiver(args.thisv()));
  if (!controller) return Error(cx,"error requires a ReadableStreamDefaultController");
  JSAutoRealm realm(cx,controller);
  JS::RootedValue reason(cx,args.get(0));
  if (!JS_WrapValue(cx,&reason) || !DefaultError(cx,controller,reason)) return false;
  args.rval().setUndefined();return true;
}
}
bool InstallDefaultController(State& state) {
  JS::RootedObject parent(state.cx,JS::GetRealmObjectPrototype(state.cx));
  auto& kit=state.kit;
  auto& face=state.defaultController;
  return parent && kit.Define("ReadableStreamDefaultController",&kDefaultControllerClass,
      parent,Construct,&face) &&
      kit.Method(face,"ReadableStreamDefaultController","close",Close,0,0) &&
      kit.Method(face,"ReadableStreamDefaultController","enqueue",Enqueue,0,0) &&
      kit.Method(face,"ReadableStreamDefaultController","error",Fail,0,0) &&
      kit.Accessor(face,"ReadableStreamDefaultController","desiredSize",Desired,nullptr,0);
}
}
