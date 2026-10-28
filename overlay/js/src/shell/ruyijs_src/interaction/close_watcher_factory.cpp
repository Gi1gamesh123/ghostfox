// Public conversion stays in the called constructor realm; publication is last.
#include "interaction/close_watcher.h"
#include "runtime/native_constructor_trace.h"
#include "jsfriendapi.h"
#include "js/GlobalObject.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::interaction {
namespace {
bool Prototype(JSContext* cx,const JS::CallArgs& args,JS::MutableHandleObject result) {
  JS::RootedObject target(cx,&args.newTarget().toObject());JS::RootedValue value(cx);
  if (!JS_GetProperty(cx,target,"prototype",&value)) return false;
  if (value.isObject()) {result.set(&value.toObject());return true;}
  JS::Realm* realm=JS::GetFunctionRealm(cx,target);if (!realm) return false;
  JSObject* global=JS::GetRealmGlobalOrNull(realm);
  for (State* owner=activeState;owner;owner=owner->previous) if (owner->page==global && owner->closeFactory) {
    result.set(CloseObject(owner->closeFactory,FProto));return JS_WrapObject(cx,result);
  }
  return Error(cx,"TypeError","CloseWatcher target realm is not installed");
}
bool AbortStep(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject watcher(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
  args.rval().setUndefined();
  if (CloseOwner(watcher)) {JSAutoRealm realm(cx,watcher);RemoveCloseWatcher(watcher);}
  return true;
}
bool ConvertSignal(State& state,JS::HandleValue options,JS::MutableHandleValue signal) {
  JSContext* cx=state.cx;
  if (!options.isNullOrUndefined() && !options.isObject())
    return Error(cx,"TypeError","CloseWatcher constructor: Argument 1 can't be converted to a dictionary.");
  if (options.isObject()) {
    JS::RootedObject object(cx,&options.toObject());
    if (!JS_GetProperty(cx,object,"signal",signal)) return false;
  }
  if (signal.isUndefined()) return true;
  if (!signal.isObject()) return Error(cx,"TypeError","CloseWatcher constructor: 'signal' member of CloseWatcherOptions is not an object.");
  return (state.abortPorts.brand && state.abortPorts.brand(signal)) ||
    Error(cx,"TypeError","CloseWatcher constructor: 'signal' member of CloseWatcherOptions does not implement interface AbortSignal.");
}
}
bool CloseWatcherConstruct(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);NativeConstructorTrace trace(cx,args);
  if (!args.isConstructing()) return Error(cx,"TypeError","CloseWatcher constructor: 'new' is required");
  State* state=RealmState(cx);
  if (!state || !state->closeFactory) return Error(cx,"TypeError","CloseWatcher realm is not installed");
  state->Observe("CloseWatcher","constructor",int(ApiAccess::Construct));
  JS::RootedObject prototype(cx);JS::RootedValue signal(cx);
  if (!Prototype(cx,args,&prototype) || !ConvertSignal(*state,args.get(0),&signal)) return false;
  if (!CloseFullyActive(*state)) {
    if (!state->exceptionReporter) return Error(cx,"TypeError","CloseWatcher exception owner is unavailable");
    return state->exceptionReporter(state->exceptionContext,"InvalidStateError","CloseWatcher constructor: The document is not fully active.");
  }
  JS::RootedObject watcher(cx,JS_NewObjectWithGivenProto(cx,&kCloseWatcherClass,prototype));
  if (!watcher || !AttachCloseWatcher(*state,watcher)) return false;
  JS::SetReservedSlot(watcher,CRunning,JS::BooleanValue(false));
  JS::SetReservedSlot(watcher,COnCancel,JS::NullValue());JS::SetReservedSlot(watcher,COnClose,JS::NullValue());
  if (!signal.isUndefined() && state->abortPorts.aborted(signal)) {
    args.rval().setObject(*watcher);return trace.Finish(true);
  }
  JS::RootedObject manager(cx,state->closeManager),group(cx);
  if (!PrepareCloseGroup(cx,manager,&group)) return false;
  if (!signal.isUndefined()) {
    JS::RootedFunction fn(cx,js::NewFunctionWithReserved(cx,AbortStep,0,0,nullptr));
    if (!fn) return false;
    JS::RootedObject callback(cx,JS_GetFunctionObject(fn));
    js::SetFunctionNativeReserved(callback,0,JS::ObjectValue(*watcher));
    JS::RootedValue value(cx,JS::ObjectValue(*callback));
    if (!state->abortPorts.follow || !state->abortPorts.follow(cx,signal,value)) return false;
  }
  PublishCloseWatcher(manager,group,watcher);
  args.rval().setObject(*watcher);return trace.Finish(true);
}
}
