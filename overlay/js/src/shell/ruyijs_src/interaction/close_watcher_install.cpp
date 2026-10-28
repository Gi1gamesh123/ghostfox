// Window's original derived installer owns this sole CloseWatcher interface.
#include "interaction/close_watcher.h"
#include "runtime/page_helpers.h"
#include "jsfriendapi.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::interaction {
namespace {const JSClass kFactory={"Interaction CloseWatcher factory",JSCLASS_HAS_RESERVED_SLOTS(FSlots)};}
bool InstallCloseWatcher(State& state) {
  if (state.closeFactory) return true;
  JSContext* cx=state.cx;
  static const JSFunctionSpec methods[]={
    JS_FN("requestClose",CloseRequestCall,0,JSPROP_ENUMERATE),
    JS_FN("close",CloseCloseCall,0,JSPROP_ENUMERATE),
    JS_FN("destroy",CloseDestroyCall,0,JSPROP_ENUMERATE),JS_FS_END};
  static const JSPropertySpec properties[]={
    JS_PSGS("oncancel",CloseCancelGet,CloseCancelSet,JSPROP_ENUMERATE),
    JS_PSGS("onclose",CloseCloseGet,CloseCloseSet,JSPROP_ENUMERATE),JS_PS_END};
  JS::RootedObject factory(cx,JS_NewObjectWithGivenProto(cx,&kFactory,nullptr)),manager(cx);
  if (!factory || !NewCloseManager(cx,&manager)) return false;
  JS::RootedObject prototype(cx,JS_NewPlainObject(cx));
  JS::RootedFunction function(cx,JS_NewFunction(cx,CloseWatcherConstruct,0,JSFUN_CONSTRUCTOR,"CloseWatcher"));
  if (!prototype || !function) return false;
  JS::RootedObject constructor(cx,JS_GetFunctionObject(function));
  JS::RootedValue metadata(cx);
  if (!JS_GetProperty(cx,constructor,"length",&metadata) ||
      !JS_GetProperty(cx,constructor,"name",&metadata) ||
      !JS_DefineProperty(cx,constructor,"prototype",prototype,JSPROP_READONLY|JSPROP_PERMANENT) ||
      !JS_SetPrototype(cx,prototype,state.targetPrototype) ||
      !JS_DefineFunctions(cx,prototype,methods) || !JS_DefineProperties(cx,prototype,properties)) return false;
  JS::RootedValue ctor(cx,JS::ObjectValue(*constructor)),parent(cx),add(cx),remove(cx);
  if (!JS_GetProperty(cx,state.targetPrototype,"constructor",&parent) ||
      !JS_GetProperty(cx,state.targetPrototype,"addEventListener",&add) ||
      !JS_GetProperty(cx,state.targetPrototype,"removeEventListener",&remove)) return false;
  JS::RootedObject parentConstructor(cx,&parent.toObject());
  if (!JS_SetPrototype(cx,constructor,parentConstructor) ||
      !JS_DefineProperty(cx,prototype,"constructor",ctor,0) || !Tag(cx,prototype,"CloseWatcher") ||
      !JS_DefineProperty(cx,state.page,"CloseWatcher",constructor,0)) return false;
  JS::SetReservedSlot(factory,FCtor,ctor);JS::SetReservedSlot(factory,FProto,JS::ObjectValue(*prototype));
  JS::SetReservedSlot(factory,FAdd,add);JS::SetReservedSlot(factory,FRemove,remove);
  for (bool close:{false,true}) {
    JS::RootedFunction fn(cx,js::NewFunctionWithReserved(cx,CloseHandlerInvoke,1,0,nullptr));if (!fn) return false;
    JS::RootedObject function(cx,JS_GetFunctionObject(fn));
    js::SetFunctionNativeReserved(function,0,JS::BooleanValue(close));
    JS::SetReservedSlot(factory,close?FCloseInvoker:FCancelInvoker,JS::ObjectValue(*function));
  }
  if (!state.helpers->Interface("CloseWatcher",prototype) || !state.helpers->Constructor("CloseWatcher",constructor)) return false;
  state.closeFactory=factory;state.closeManager=manager;return true;
}
}
