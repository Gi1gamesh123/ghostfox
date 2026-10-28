#include "resource_network/writable_stream.h"
#include "resource_network/state.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
namespace {
bool Illegal(JSContext* cx,unsigned,JS::Value*) {return Error(cx,"Illegal constructor.");}
bool ControllerError(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject controller(cx,SinkReceiver(args.thisv()));
  if (!controller) return Error(cx,"'error' called on an object that does not implement interface WritableStreamDefaultController.");
  args.rval().setUndefined();return WritableControllerError(cx,controller,args.get(0));
}
bool Signal(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject controller(cx,SinkReceiver(args.thisv()));
  if (!controller) return Error(cx,"'get signal' called on an object that does not implement interface WritableStreamDefaultController.");
  args.rval().set(JS::GetReservedSlot(controller,KSignal));return JS_WrapValue(cx,args.rval());
}
}
bool InstallSinkController(State& state) {
  JS::RootedObject parent(state.cx,JS::GetRealmObjectPrototype(state.cx));auto& k=state.kit;auto& face=state.writableController;
  return parent && k.Define("WritableStreamDefaultController",&kSinkControllerClass,parent,Illegal,&face) &&
      k.Method(face,"WritableStreamDefaultController","error",ControllerError,0,0) &&
      k.Accessor(face,"WritableStreamDefaultController","signal",Signal,nullptr,0);
}
}
