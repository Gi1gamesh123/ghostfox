// WakeLock / WakeLockSentinel (dom/webidl/WakeLock.webidl) and navigator.wakeLock,
// following donor dom/power/WakeLockJS.cpp (request steps, NotAllowedError
// messages, the queued obtain task) and WakeLockSentinel.cpp (release: mark
// released, fire a trusted "release" event, resolve undefined).
#include "web_wake_lock/internal.h"
#include "browser_environment/environment.h"
#include "interaction/realm.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "runtime/runtime.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include <string>

namespace ruyijs::web_wake_lock {
const JSClass kWakeLockClass = {"WakeLock", JSCLASS_HAS_RESERVED_SLOTS(1)};
const JSClass kSentinelClass = {"WakeLockSentinel", JSCLASS_HAS_RESERVED_SLOTS(SentinelSlotCount)};



namespace {
bool Illegal(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return trace.Finish(WebKit::Fail(cx, "Illegal constructor."));
}
}  // namespace
bool RejectPending(JSContext* cx, JS::MutableHandleValue rval) {
  JS::RootedValue error(cx);
  if (!JS_GetPendingException(cx, &error)) return false;
  JS_ClearPendingException(cx);
  JSObject* promise = JS::CallOriginalPromiseReject(cx, error);
  if (!promise) return false;
  rval.setObject(*promise);
  return true;
}
namespace {
bool NavigatorWakeLock(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedValue navigator(cx);
  JSObject* self = args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr;
  {
    JSAutoRealm realm(cx, s.kit.page);
    if (!JS_GetProperty(cx, s.kit.page, "navigator", &navigator)) return false;
  }
  if (!self || !navigator.isObject() ||
      JS::GetClass(self) != JS::GetClass(js::UncheckedUnwrap(&navigator.toObject())))
    return WebKit::Fail(cx, Incompatible("get wakeLock", "Navigator").c_str());
  JS::RootedValue value(cx, JS::ObjectValue(*s.instance));
  if (!JS_WrapValue(cx, &value)) return false;
  args.rval().set(value);
  return true;
}
// Step 7.3: the queued obtain task. The document stays visible in this
// runtime, so the sentinel is always granted.
bool ObtainTask(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject callee(cx, &args.callee());
  State& s = *static_cast<State*>(js::GetFunctionNativeReserved(callee, 0).toPrivate());
  JS::RootedObject promise(cx, &js::GetFunctionNativeReserved(callee, 1).toObject());
  args.rval().setUndefined();
  JSAutoRealm realm(cx, s.kit.page);
  JS::RootedObject sentinel(cx, s.kit.New(&kSentinelClass, s.sentinel.prototype));
  if (!sentinel || !s.events.Attach(sentinel)) return false;
  JS::SetReservedSlot(sentinel, SentinelReleased, JS::BooleanValue(false));
  JS::SetReservedSlot(sentinel, SentinelHandler, JS::NullValue());
  JS::RootedValue value(cx, JS::ObjectValue(*sentinel));
  return JS::ResolvePromise(cx, promise, value);
}
bool Request(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx);
  if (!WebKit::Check(cx, args.thisv(), &kWakeLockClass, &self, Incompatible("request", "WakeLock").c_str()))
    return RejectPending(cx, args.rval());
  if (!args.get(0).isUndefined()) {
    JS::RootedString text(cx, JS::ToString(cx, args[0]));
    std::string type;
    if (!text || !ToUTF8(cx, text, &type)) return RejectPending(cx, args.rval());
    if (type != "screen") {
      (void)WebKit::Fail(cx, ("WakeLock.request: '" + type +
          "' (value of argument 1) is not a valid value for enumeration WakeLockType.").c_str());
      return RejectPending(cx, args.rval());
    }
  }
  JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!promise) return false;
  args.rval().setObject(*promise);
  JS::RootedValue visibility(cx), document(cx);
  {
    JSAutoRealm realm(cx, s.kit.page);
    if (!JS_GetProperty(cx, s.kit.page, "document", &document)) return false;
    if (document.isObject()) {
      JS::RootedObject doc(cx, &document.toObject());
      if (!JS_GetProperty(cx, doc, "hidden", &visibility)) return false;
    }
  }
  if (visibility.isTrue()) {
    JS::RootedValue constructor(cx, JS::ObjectValue(*s.exception)), error(cx);
    JS::RootedString message(cx, JS_NewStringCopyZ(cx, "The requesting document is hidden.")),
        name(cx, JS_NewStringCopyZ(cx, "NotAllowedError"));
    JS::RootedValueArray<2> values(cx);
    JS::RootedObject created(cx);
    if (!message || !name) return false;
    values[0].setString(message);
    values[1].setString(name);
    if (!JS::Construct(cx, constructor, values, &created)) return false;
    error.setObject(*created);
    return JS::RejectPromise(cx, promise, error);
  }
  JSFunction* function = js::NewFunctionWithReserved(cx, ObtainTask, 0, 0, "ObtainWakeLock");
  if (!function) return false;
  JS::RootedObject task(cx, JS_GetFunctionObject(function));
  js::SetFunctionNativeReserved(task, 0, JS::PrivateValue(&s));
  js::SetFunctionNativeReserved(task, 1, JS::ObjectValue(*promise));
  return !s.taskPort || s.taskPort(s.taskContext, task);
}
}  // namespace

Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, interaction::Realm& events,
             browser_environment::Environment& environment)
    : state_(std::make_unique<State>(cx, page, helpers, events, environment)) {}
Realm::~Realm() = default;
void Realm::SetTaskPort(TaskPort port, void* context) {
  state_->taskPort = port;
  state_->taskContext = context;
}

bool Realm::Install() {
  State& s = *state_;
  JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.kit.page);
  JS::RootedValue exception(cx);
  if (!JS_GetProperty(cx, s.kit.page, "DOMException", &exception) || !exception.isObject())
    return WebKit::Fail(cx, "WakeLock requires the realm's DOMException");
  s.exception = &exception.toObject();
  JS::RootedObject objectPrototype(cx, JS::GetRealmObjectPrototype(cx)), targetPrototype(cx, s.events.prototype());
  if (!objectPrototype || !targetPrototype ||
      !s.kit.Define("WakeLock", &kWakeLockClass, objectPrototype, Illegal, &s.wakeLock) ||
      !s.kit.Method(s.wakeLock, "WakeLock", "request", Request, 0, 0) ||
      !s.kit.Define("WakeLockSentinel", &kSentinelClass, targetPrototype, Illegal, &s.sentinel) ||
      !s.kit.Method(s.sentinel, "WakeLockSentinel", "release", Release, 0, 0) ||
      !s.kit.Accessor(s.sentinel, "WakeLockSentinel", "released", Released, nullptr, 0) ||
      !s.kit.Accessor(s.sentinel, "WakeLockSentinel", "type", Type, nullptr, 0) ||
      !s.kit.Accessor(s.sentinel, "WakeLockSentinel", "onrelease", HandlerGet, HandlerSet, 0)) return false;
  s.instance = s.kit.New(&kWakeLockClass, s.wakeLock.prototype);
  if (!s.instance) return false;
  JS::RootedObject navigatorPrototype(cx, s.environment.navigatorPrototype());
  JS::RootedObject getter(cx, s.kit.Function(NavigatorWakeLock, "get wakeLock", 0, 0)), setter(cx);
  return navigatorPrototype && getter &&
         JS_DefineProperty(cx, navigatorPrototype, "wakeLock", getter, setter, JSPROP_ENUMERATE) &&
         s.kit.helpers->Accessor("Navigator", navigatorPrototype, "wakeLock") &&
         s.kit.helpers->Object("WakeLock", s.instance) &&
         s.secure.Add(navigatorPrototype, "wakeLock") && s.secure.Add(s.kit.page, "WakeLock") &&
         s.secure.Add(s.kit.page, "WakeLockSentinel");
}

bool Realm::ConfigureExposure() {
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.kit.page);
  JS::RootedValue secure(s.cx);
  return JS_GetProperty(s.cx, s.kit.page, "isSecureContext", &secure) && s.secure.Apply(secure.isTrue());
}
}  // namespace ruyijs::web_wake_lock
