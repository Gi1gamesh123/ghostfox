#include "web_workers/child.h"
#include "web_workers/handlers.h"
#include "web_workers/error_event.h"
#include "interaction/realm.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::web_workers {
bool Child::GlobalMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto& child = *static_cast<Child*>(WebKit::Owner(args));
  if (!args.thisv().isNullOrUndefined() &&
      (!args.thisv().isObject() || &args.thisv().toObject() != child.kit_.page))
    return WebKit::Fail(cx, "Worker global attribute receiver is invalid");
  switch (WebKit::Slot(args)) {
    case 0: args.rval().setObject(*child.kit_.page); return true;
    case 1: args.rval().setString(child.name_); return true;
    case 2:
      if (!argc) return WebKit::Fail(cx, "Worker name setter requires a value");
      args.rval().setUndefined();
      return JS_DefineProperty(cx, child.kit_.page, "name", args[0], JSPROP_ENUMERATE);
  }
  return WebKit::Fail(cx, "Unknown Worker global attribute");
}

bool Child::Install() {
  JS::RootedObject eventPrototype(cx_, events_.prototype());
  JS::RootedValue eventConstructor(cx_);
  if (!eventPrototype || !JS_GetProperty(cx_, eventPrototype, "constructor", &eventConstructor) ||
      !eventConstructor.isObject()) return false;
  JS::RootedObject eventCtor(cx_, &eventConstructor.toObject());
  if (!kit_.Define("WorkerGlobalScope", nullptr, eventPrototype, WebKit::Reject, &globalScope_) ||
      !JS_SetPrototype(cx_, globalScope_.constructor, eventCtor)) return false;
  if (service()) {
    // ServiceWorkerGlobalScope belongs to the Service Worker owner, which moves the
    // global onto it; until then WorkerGlobalScope members (isSecureContext, read by
    // every [SecureContext] exposure) resolve through this prototype.
    if (!JS_SetPrototype(cx_, kit_.page, globalScope_.prototype)) return false;
    handlers_ = NewHandlerStore(cx_, eventPrototype);
    errorPrototype_ = ErrorPrototype(cx_, kit_.page);
    name_ = JS_GetEmptyString(cx_);
    return errorPrototype_ && handlers_ && name_ &&
        InstallHandlers(cx_, *kit_.helpers, globalScope_.prototype, handlers_, kit_.page, HandlerExposure::GlobalScope) &&
        kit_.Accessor(globalScope_, "WorkerGlobalScope", "self", GlobalMember, nullptr, 0) &&
        InstallScopeMethods();
  }
  const char* scopeName = shared() ? "SharedWorkerGlobalScope" : "DedicatedWorkerGlobalScope";
  WebFace& scope = shared() ? sharedScope_ : dedicatedScope_;
  if (!kit_.Define(scopeName, nullptr, globalScope_.prototype, WebKit::Reject, &scope) ||
      !JS_SetPrototype(cx_, scope.constructor, globalScope_.constructor)) return false;
  bool immutable = false;
  if (!JS_SetPrototype(cx_, kit_.page, scope.prototype) ||
      !JS_SetImmutablePrototype(cx_, kit_.page, &immutable) || !immutable) return false;
  handlers_ = NewHandlerStore(cx_, eventPrototype);
  errorPrototype_ = ErrorPrototype(cx_, kit_.page);
  name_ = JS_GetEmptyString(cx_);
  if (!errorPrototype_ || !handlers_ || !name_ ||
      !InstallHandlers(cx_, *kit_.helpers, globalScope_.prototype, handlers_, kit_.page, HandlerExposure::GlobalScope) ||
      !InstallHandlers(cx_, *kit_.helpers, kit_.page, handlers_, kit_.page,
                       shared() ? HandlerExposure::SharedScope : HandlerExposure::DedicatedScope) ||
      !kit_.Accessor(globalScope_, "WorkerGlobalScope", "self", GlobalMember, nullptr, 0) ||
      !InstallScopeMethods()) return false;
  JS::RootedObject close(cx_, kit_.Function(Close, "close", 0, 0));
  JS::RootedObject getName(cx_, kit_.Function(GlobalMember, "get name", 0, 1));
  JS::RootedObject setName(cx_, kit_.Function(GlobalMember, "set name", 1, 2));
  if (!close || !getName || !setName ||
      !JS_DefineProperty(cx_, kit_.page, "close", close, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx_, kit_.page, "name", getName, setName, JSPROP_ENUMERATE) ||
      !kit_.helpers->Member(close, scopeName, "close", "call") ||
      !kit_.helpers->Accessor(scopeName, kit_.page, "name")) return false;
  if (shared()) {
    // SharedWorkerGlobalScope has no postMessage and no AnimationFrameProvider
    // (Firefox 158): remove requestAnimationFrame that Scheduling installed on
    // the global before the shared scope was chosen.
    for (const char* name : {"requestAnimationFrame", "cancelAnimationFrame"})
      if (!JS_DeleteProperty(cx_, kit_.page, name)) return false;
    return true;
  }
  JS::RootedObject post(cx_, kit_.Function(Send, "postMessage", 1, 0));
  return post &&
      JS_DefineProperty(cx_, kit_.page, "postMessage", post, JSPROP_ENUMERATE) &&
      kit_.helpers->Member(post, "DedicatedWorkerGlobalScope", "postMessage", "call");
}
}
