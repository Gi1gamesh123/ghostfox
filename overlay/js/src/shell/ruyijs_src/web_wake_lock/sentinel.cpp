// WakeLockSentinel (donor dom/power/WakeLockSentinel.cpp): release marks the
// sentinel released, fires a trusted "release" event once and resolves undefined.
#include "web_wake_lock/internal.h"
#include "interaction/realm.h"
#include "jsapi.h"
#include "js/CallAndConstruct.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::web_wake_lock {
namespace {
JSObject* Sentinel(JSContext* cx, const JS::CallArgs& args, const char* member) {
  JS::RootedObject self(cx);
  if (!WebKit::Check(cx, args.thisv(), &kSentinelClass, &self, Incompatible(member, "WakeLockSentinel").c_str()))
    return nullptr;
  return self;
}
}  // namespace
bool Release(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx, Sentinel(cx, args, "release"));
  if (!self) return RejectPending(cx, args.rval());
  if (!JS::GetReservedSlot(self, SentinelReleased).toBoolean()) {
    JS::SetReservedSlot(self, SentinelReleased, JS::BooleanValue(true));
    JSAutoRealm realm(cx, s.kit.page);
    if (!s.events.Fire(self, "release", false, false)) return false;
  }
  JSObject* resolved = JS::CallOriginalPromiseResolve(cx, JS::UndefinedHandleValue);
  if (!resolved) return false;
  args.rval().setObject(*resolved);
  return true;
}
bool Released(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JSObject* self = Sentinel(cx, args, "get released");
  if (!self) return false;
  args.rval().set(JS::GetReservedSlot(self, SentinelReleased));
  return true;
}
bool Type(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!Sentinel(cx, args, "get type")) return false;
  JSString* text = JS_NewStringCopyZ(cx, "screen");
  if (!text) return false;
  args.rval().setString(text);
  return true;
}
// onrelease: an EventHandler attribute over the sentinel's own listener list.
bool HandlerGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JSObject* self = Sentinel(cx, args, "get onrelease");
  if (!self) return false;
  args.rval().set(JS::GetReservedSlot(self, SentinelHandler));
  return JS_WrapValue(cx, args.rval());
}
bool HandlerSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx, Sentinel(cx, args, "set onrelease"));
  if (!self) return false;
  JS::RootedValue previous(cx, JS::GetReservedSlot(self, SentinelHandler)), desired(cx, args.get(0));
  if (!desired.isObject() || !JS::IsCallable(&desired.toObject())) desired.setNull();
  JSAutoRealm realm(cx, s.kit.page);
  if (!JS_WrapValue(cx, &desired) || !JS_WrapValue(cx, &previous)) return false;
  JS::RootedObject prototype(cx, s.events.prototype());
  JS::RootedValue function(cx), receiver(cx, JS::ObjectValue(*self)), ignored(cx);
  JS::RootedString type(cx, JS_NewStringCopyZ(cx, "release"));
  if (!type) return false;
  JS::RootedValueArray<2> parameters(cx);
  parameters[0].setString(type);
  for (int operation = 0; operation < 2; ++operation) {
    parameters[1].set(operation ? desired : previous);
    if (!parameters[1].isObject()) continue;
    if (!JS_GetProperty(cx, prototype, operation ? "addEventListener" : "removeEventListener", &function) ||
        !JS::Call(cx, receiver, function, parameters, &ignored)) return false;
  }
  JS::SetReservedSlot(self, SentinelHandler, desired);
  args.rval().setUndefined();
  return true;
}
}  // namespace ruyijs::web_wake_lock
