// CloseWatcher public methods use the original event and membership authorities.
#include "interaction/close_watcher.h"
#include "js/Realm.h"

namespace ruyijs::interaction {
JSObject* CloseReceiver(JSContext* cx,JS::HandleValue value,const char* name) {
  JSObject* object=value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr;
  if (object && JS::GetClass(object)==&kCloseWatcherClass && CloseOwner(object)) return object;
  // Keep exact method-realm TypeError without evaluating receiver properties.
  static const JSErrorFormatString format={"RUYIJS_CLOSE_RECEIVER","'{0}' called on an object that does not implement interface CloseWatcher.",1,JSEXN_TYPEERR};
  JS_ReportErrorNumberASCII(cx,[](void*,unsigned){return &format;},nullptr,0,name);return nullptr;
}
bool CloseFullyActive(State& state) {return state.closePorts.active && state.closePorts.active(state.closeContext);}
bool CloseIsActive(State& state,JSObject* watcher) {
  return CloseObject(watcher,CManager)==state.closeManager && CloseObject(watcher,CGroup) && CloseFullyActive(state);
}
bool CloseWatcherNow(State& state,JS::HandleObject watcher) {
  if (!CloseIsActive(state,watcher)) return true;
  RemoveCloseWatcher(watcher);
  return FireTargetEvent(state,watcher,"close",false,false);
}
bool RequestCloseWatcher(State& state,JS::HandleObject watcher,bool history,bool* proceed) {
  *proceed=true;
  if (!CloseIsActive(state,watcher) || JS::GetReservedSlot(watcher,CRunning).isTrue()) return true;
  JS::RootedObject manager(state.cx,CloseObject(watcher,CManager));
  const bool cancelable=!history || (CloseCanGrow(manager) && state.closePorts.history &&
                                    state.closePorts.history(state.closeContext));
  bool allowed=true;
  {
    JS::SetReservedSlot(watcher,CRunning,JS::BooleanValue(true));
    struct Guard {JS::HandleObject watcher;~Guard(){JS::SetReservedSlot(watcher,CRunning,JS::BooleanValue(false));}} guard{watcher};
    if (!FireTargetEvent(state,watcher,"cancel",false,cancelable,&allowed)) return false;
  }
  if (!allowed) {
    // A reentrant document commit must not consume its fresh activation value.
    if (manager==state.closeManager && state.closePorts.consume) state.closePorts.consume(state.closeContext);
    *proceed=false;return true;
  }
  return CloseWatcherNow(state,watcher);
}
namespace {
bool Method(JSContext* cx,unsigned argc,JS::Value* vp,int op) {
  auto args=JS::CallArgsFromVp(argc,vp);
  const char* name=op==0?"requestClose":op==1?"close":"destroy";
  JS::RootedObject watcher(cx,CloseReceiver(cx,args.thisv(),name));if (!watcher) return false;
  auto& state=*CloseOwner(watcher);state.Observe("CloseWatcher",name,int(ApiAccess::Call));
  JSAutoRealm realm(cx,watcher);args.rval().setUndefined();bool proceed=true;
  if (!op) return RequestCloseWatcher(state,watcher,false,&proceed);
  if (op==1) return CloseWatcherNow(state,watcher);
  RemoveCloseWatcher(watcher);return true;
}
}
bool CloseRequestCall(JSContext* cx,unsigned n,JS::Value* vp){return Method(cx,n,vp,0);}
bool CloseCloseCall(JSContext* cx,unsigned n,JS::Value* vp){return Method(cx,n,vp,1);}
bool CloseDestroyCall(JSContext* cx,unsigned n,JS::Value* vp){return Method(cx,n,vp,2);}
}
