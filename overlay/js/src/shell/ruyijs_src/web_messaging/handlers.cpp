#include "web_messaging/state.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/ValueArray.h"

namespace ruyijs::web_messaging {
namespace {
bool Invoke(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject port(cx, &js::GetFunctionNativeReserved(&args.callee(), 0).toObject());
  unsigned which = unsigned(js::GetFunctionNativeReserved(&args.callee(), 1).toInt32());
  JS::RootedValue handler(cx, JS::GetReservedSlot(port, OnMessage + which));
  args.rval().setUndefined();
  if (handler.isNull()) return true;
  JS::RootedValue result(cx);
  if (!JS::Call(cx, args.thisv(), handler, JS::HandleValueArray(args), &result)) return false;
  if (result.isBoolean() && !result.toBoolean()) {
    Record* record = Unwrap(port);
    JS::RootedValue prevent(cx, record->owner.prevent);
    if (!JS::Call(cx, args.get(0), prevent, JS::HandleValueArray::empty(), &result)) return false;
  }
  return true;
}
}
bool GetHandler(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Record* record = Receiver(cx, args.thisv());
  if (!record) return false;
  args.rval().set(JS::GetReservedSlot(record->port, OnMessage + WebKit::Slot(args)));
  return JS_WrapValue(cx, args.rval());
}
bool SetHandler(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Record* record = Receiver(cx, args.thisv());
  if (!record) return false;
  State& s = record->owner;
  JSAutoRealm realm(cx, s.kit.page);
  unsigned which = unsigned(WebKit::Slot(args));
  JS::RootedObject port(cx, record->port);
  JS::RootedValue value(cx, args.get(0).isObject() ? args.get(0) : JS::NullValue());
  if (!JS_WrapValue(cx, &value)) return false;
  JS::RootedValue old(cx, JS::GetReservedSlot(port, OnMessage + which));
  JS::RootedValue callback(cx, JS::GetReservedSlot(port, MessageHandler + which));
  if (value.isObject() && callback.isNull()) {
    JS::RootedFunction fn(cx, js::NewFunctionWithReserved(cx, Invoke, 1, 0, "eventHandler"));
    if (!fn) return false;
    JS::RootedObject function(cx, JS_GetFunctionObject(fn));
    js::SetFunctionNativeReserved(function, 0, JS::ObjectValue(*port));
    js::SetFunctionNativeReserved(function, 1, JS::Int32Value(which));
    callback.setObject(*function);
    JS::SetReservedSlot(port, MessageHandler + which, callback);
  }
  if (old.isNull() != value.isNull()) {
    JS::RootedString type(cx, JS_NewStringCopyZ(cx, which ? "messageerror" : "message"));
    if (!type) return false;
    JS::RootedValueArray<2> parameters(cx);
    parameters[0].setString(type); parameters[1].set(callback);
    JS::RootedValue receiver(cx, JS::ObjectValue(*port)), ignored(cx);
    JS::RootedValue method(cx, value.isNull() ? s.remove : s.add);
    if (!JS::Call(cx, receiver, method, parameters, &ignored)) return false;
  }
  JS::SetReservedSlot(port, OnMessage + which, value);
  // The IDL setter starts even when the new handler is null (Firefox baseline).
  if (!which && record->endpoint) {
    record->endpoint->enabled = true;
    if (!Queue(record->endpoint)) return false;
  }
  args.rval().setUndefined(); return true;
}
}
