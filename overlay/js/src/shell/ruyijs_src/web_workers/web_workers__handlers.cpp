#include "web_workers/handlers.h"
#include "web_workers/state.h"
#include "web_workers/error_event.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::web_workers {
namespace {
constexpr unsigned kCount = 10, kAdd = 20, kRemove = 21, kGlobal = 22, kPrevent = 23;
const JSClass kStoreClass = {"WorkerHandlers", JSCLASS_HAS_RESERVED_SLOTS(24)};
const char* const kNames[] = {"onmessage", "onmessageerror", "onerror", "onlanguagechange",
    "onoffline", "ononline", "onrejectionhandled", "onunhandledrejection", "onrtctransform",
    "onconnect"};
JSObject* Store(JSContext* cx, const JS::CallArgs& args) {
  JS::RootedObject self(cx, args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  auto slot = js::GetFunctionNativeReserved(&args.callee(), 0);
  if (slot.isObject()) {
    JSObject* store = &slot.toObject();
    if (args.thisv().isNullOrUndefined() ||
        (self && &JS::GetReservedSlot(store, kGlobal).toObject() == self)) return store;
  } else if (self && (JS::GetClass(self) == &kWorkerClass || JS::GetClass(self) == &kSharedWorkerClass)) {
    return &JS::GetReservedSlot(self, 1).toObject();
  }
  WebKit::Fail(cx, "Worker event handler receiver is invalid");
  return nullptr;
}
bool Invoke(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject store(cx, &js::GetFunctionNativeReserved(&args.callee(), 0).toObject());
  unsigned index = unsigned(js::GetFunctionNativeReserved(&args.callee(), 1).toInt32());
  JS::RootedValue handler(cx, JS::GetReservedSlot(store, index));
  args.rval().setUndefined();
  if (handler.isNull()) return true;
  JS::RootedValue result(cx);
  JS::RootedValueArray<5> fields(cx);
  const bool special = index == 2 && JS::GetReservedSlot(store, kGlobal).isObject() &&
      ErrorArguments(cx, args.get(0), fields);
  const JS::HandleValueArray arguments = special ? JS::HandleValueArray(fields) : JS::HandleValueArray(args);
  if (!JS::Call(cx, args.thisv(), handler, arguments, &result)) return false;
  if (result.isBoolean() && result.toBoolean() == special && args.get(0).isObject()) {
    JS::RootedValue prevent(cx, JS::GetReservedSlot(store, kPrevent));
    if (!JS::Call(cx, args[0], prevent, JS::HandleValueArray::empty(), &result)) return false;
  }
  return true;
}
JSObject* Function(JSContext* cx, JSNative fn, const char* name, unsigned length,
                   JS::HandleObject store, unsigned index) {
  JS::RootedFunction function(cx, js::NewFunctionWithReserved(cx, fn, length, 0, name));
  if (!function) return nullptr;
  JS::RootedObject object(cx, JS_GetFunctionObject(function));
  js::SetFunctionNativeReserved(object, 0, store ? JS::ObjectValue(*store) : JS::NullValue());
  js::SetFunctionNativeReserved(object, 1, JS::Int32Value(index));
  return object;
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject store(cx, Store(cx, args));
  if (!store) return false;
  args.rval().set(JS::GetReservedSlot(store, WebKit::Slot(args)));
  return JS_WrapValue(cx, args.rval());
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject store(cx, Store(cx, args));
  if (!store) return false;
  unsigned index = unsigned(WebKit::Slot(args));
  JS::RootedValue value(cx, args.get(0).isObject() ? args.get(0) : JS::NullValue());
  JS::RootedValue self(cx, args.thisv());
  JSAutoRealm realm(cx, store);
  if (self.isNullOrUndefined()) self = JS::GetReservedSlot(store, kGlobal);
  if (!JS_WrapValue(cx, &value) || !JS_WrapValue(cx, &self)) return false;
  JS::RootedValue old(cx, JS::GetReservedSlot(store, index));
  JS::RootedValue callback(cx, JS::GetReservedSlot(store, index + kCount));
  if (value.isObject() && callback.isNull()) {
    JS::RootedObject fn(cx, Function(cx, Invoke, "eventHandler", 1, store, index));
    if (!fn) return false;
    callback.setObject(*fn); JS::SetReservedSlot(store, index + kCount, callback);
  }
  if (old.isNull() != value.isNull()) {
    JS::RootedValue method(cx, JS::GetReservedSlot(store, value.isNull() ? kRemove : kAdd));
    JS::RootedString type(cx, JS_NewStringCopyZ(cx, kNames[index] + 2));
    if (!type) return false;
    JS::RootedValueArray<2> arguments(cx);
    arguments[0].setString(type); arguments[1].set(callback);
    JS::RootedValue result(cx);
    if (!JS::Call(cx, self, method, arguments, &result)) return false;
  }
  JS::SetReservedSlot(store, index, value);
  args.rval().setUndefined(); return true;
}
}
JSObject* NewHandlerStore(JSContext* cx, JS::HandleObject eventTarget) {
  JS::RootedObject store(cx, JS_NewObjectWithGivenProto(cx, &kStoreClass, nullptr));
  if (!store) return nullptr;
  for (unsigned i = 0; i < 24; ++i) JS::SetReservedSlot(store, i, JS::NullValue());
  JS::RootedValue method(cx);
  if (JS::GetClass(eventTarget) == &kStoreClass) {
    JS::SetReservedSlot(store, kAdd, JS::GetReservedSlot(eventTarget, kAdd));
    JS::SetReservedSlot(store, kRemove, JS::GetReservedSlot(eventTarget, kRemove));
    JS::SetReservedSlot(store, kPrevent, JS::GetReservedSlot(eventTarget, kPrevent));
    return store;
  }
  if (!JS_GetProperty(cx, eventTarget, "addEventListener", &method)) return nullptr;
  JS::SetReservedSlot(store, kAdd, method);
  if (!JS_GetProperty(cx, eventTarget, "removeEventListener", &method)) return nullptr;
  JS::SetReservedSlot(store, kRemove, method);
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  JS::RootedValue constructor(cx), prototype(cx);
  if (!JS_GetProperty(cx, global, "Event", &constructor) || !constructor.isObject()) return nullptr;
  JS::RootedObject event(cx, &constructor.toObject());
  if (!JS_GetProperty(cx, event, "prototype", &prototype) || !prototype.isObject()) return nullptr;
  event = &prototype.toObject();
  if (!JS_GetProperty(cx, event, "preventDefault", &method)) return nullptr;
  JS::SetReservedSlot(store, kPrevent, method);
  return store;
}
bool InstallHandlers(JSContext* cx, PageHelpers& helpers, JS::HandleObject target,
                     JS::HandleObject store, JS::HandleObject global, HandlerExposure exposure) {
  if (store) JS::SetReservedSlot(store, kGlobal, JS::ObjectValue(*global));
  for (unsigned index = 0; index < kCount; ++index) {
    const bool included = exposure == HandlerExposure::Worker ? index < 3 :
        exposure == HandlerExposure::SharedWorker ? index == 2 :
        exposure == HandlerExposure::GlobalScope ? index >= 2 && index < 8 :
        exposure == HandlerExposure::SharedScope ? index == 9 : index < 2 || index == 8;
    if (!included) continue;
    std::string getName = std::string("get ") + kNames[index];
    std::string setName = std::string("set ") + kNames[index];
    JS::RootedObject get(cx, Function(cx, Get, getName.c_str(), 0, store, index));
    JS::RootedObject set(cx, Function(cx, Set, setName.c_str(), 1, store, index));
    if (!get || !set || !JS_DefineProperty(cx, target, kNames[index], get, set, JSPROP_ENUMERATE) ||
        !helpers.Accessor(exposure == HandlerExposure::Worker ? "Worker" :
          exposure == HandlerExposure::SharedWorker ? "SharedWorker" :
          exposure == HandlerExposure::GlobalScope ? "WorkerGlobalScope" :
          exposure == HandlerExposure::SharedScope ? "SharedWorkerGlobalScope" : "DedicatedWorkerGlobalScope",
          target, kNames[index])) return false;
  }
  return true;
}
}
