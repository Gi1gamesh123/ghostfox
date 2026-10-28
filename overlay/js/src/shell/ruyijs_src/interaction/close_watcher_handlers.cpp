// Shared invokers retain a selector only; the dispatch receiver owns its handler.
#include "interaction/close_watcher.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/Realm.h"

namespace ruyijs::interaction {
bool CloseHandlerInvoke(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  const bool close=js::GetFunctionNativeReserved(&args.callee(),0).toBoolean();
  args.rval().setUndefined();
  JS::RootedObject watcher(cx,args.thisv().isObject()?js::CheckedUnwrapStatic(&args.thisv().toObject()):nullptr);
  if (!watcher || JS::GetClass(watcher)!=&kCloseWatcherClass || !CloseOwner(watcher)) return true;
  JSAutoRealm realm(cx,watcher);
  JS::RootedValue handler(cx,JS::GetReservedSlot(watcher,close?COnClose:COnCancel)),event(cx,args.get(0)),result(cx);
  if (!handler.isObject() || !JS::IsCallable(&handler.toObject())) return true;
  if (!JS_WrapValue(cx,&event) || !JS::Call(cx,watcher,handler,JS::HandleValueArray(event),&result)) return false;
  if (result.isFalse() && event.isObject()) {
    JS::RootedObject object(cx,js::CheckedUnwrapStatic(&event.toObject()));
    State* owner=object?Owner(cx,object,true):nullptr;
    if (owner) owner->event(object)->value.prevent_default();
  }
  return true;
}
namespace {
bool Handler(JSContext* cx,unsigned argc,JS::Value* vp,bool close,bool setter) {
  auto args=JS::CallArgsFromVp(argc,vp);const char* name=close?"onclose":"oncancel";
  JS::RootedObject watcher(cx,CloseReceiver(cx,args.thisv(),name));if (!watcher) return false;
  auto& state=*CloseOwner(watcher);state.Observe("CloseWatcher",name,int(setter?ApiAccess::Write:ApiAccess::Read));
  const unsigned slot=close?COnClose:COnCancel;
  if (!setter) {args.rval().set(JS::GetReservedSlot(watcher,slot));return JS_WrapValue(cx,args.rval());}
  if (!argc) return Error(cx,"TypeError",close?"onclose setter requires an argument":"oncancel setter requires an argument");
  JSAutoRealm realm(cx,watcher);
  JS::RootedValue next(cx,args[0].isObject()?args[0]:JS::NullValue()),previous(cx,JS::GetReservedSlot(watcher,slot));
  if (!JS_WrapValue(cx,&next)) return false;
  if (next.isNull()!=previous.isNull()) {
    JS::RootedObject factory(cx,state.closeFactory);
    JS::RootedValue method(cx,JS::GetReservedSlot(factory,next.isNull()?FRemove:FAdd));
    JS::RootedString type(cx,JS_NewStringCopyZ(cx,close?"close":"cancel"));if (!type) return false;
    JS::RootedValueArray<2> parameters(cx);parameters[0].setString(type);
    parameters[1].set(JS::GetReservedSlot(factory,close?FCloseInvoker:FCancelInvoker));
    JS::RootedValue result(cx);
    if (!JS::Call(cx,watcher,method,parameters,&result)) return false;
  }
  JS::SetReservedSlot(watcher,slot,next);args.rval().setUndefined();return true;
}
}
bool CloseCancelGet(JSContext* cx,unsigned n,JS::Value* vp){return Handler(cx,n,vp,false,false);}
bool CloseCancelSet(JSContext* cx,unsigned n,JS::Value* vp){return Handler(cx,n,vp,false,true);}
bool CloseCloseGet(JSContext* cx,unsigned n,JS::Value* vp){return Handler(cx,n,vp,true,false);}
bool CloseCloseSet(JSContext* cx,unsigned n,JS::Value* vp){return Handler(cx,n,vp,true,true);}
}
