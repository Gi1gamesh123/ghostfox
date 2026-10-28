#include "resource_network/websocket.h"
#include "domain/browser_environment/url_value.h"
#include "runtime/native_constructor_trace.h"
#include "interaction/realm.h"
#include "js/CallAndConstruct.h"
#include "js/Object.h"
#include "js/Wrapper.h"

namespace ruyijs::resource_network {
namespace {
const char* const kReceiver = "WebSocket member called on an incompatible receiver";
bool Delegate(JSContext* cx, JS::HandleObject self, const char* method,
              const char* type, JS::HandleValue callback) {
  JS::RootedValue fn(cx);
  if (!JS_GetProperty(cx, self, method, &fn)) return false;
  if (!fn.isObject() || !JS::IsCallable(&fn.toObject())) return true;
  JS::RootedString name(cx, JS_NewStringCopyZ(cx, type));
  if (!name) return false;
  JS::RootedValueArray<2> argv(cx);
  argv[0].setString(name); argv[1].set(callback);
  JS::RootedValue ignored(cx);
  return JS::Call(cx, self, fn, argv, &ignored);
}
bool Receiver(JSContext* cx, const JS::CallArgs& args, JS::MutableHandleObject out) {
  return WebKit::Check(cx, args.thisv(), &kWebSocketClass, out, kReceiver);
}
}

const JSClass kWebSocketClass = {"WebSocket", JSCLASS_HAS_RESERVED_SLOTS(WSlotCount)};

bool WebSocketConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  if (!args.isConstructing()) return WebKit::Fail(cx, "WebSocket requires new");
  if (!argc) return WebKit::Fail(cx, "WebSocket requires a URL");
  std::string url;
  if (!Convert(cx, args[0], &url)) return false;
  ruyijs::domain::browser_environment::UrlValue parsed;
  if (!ruyijs::domain::browser_environment::UrlValue::Resolve(url, "", &parsed) ||
      (parsed.protocol() != "ws:" && parsed.protocol() != "wss:"))
    return WebKit::Fail(cx, "The URL's scheme must be ws or wss");
  url = parsed.href();
  JS::RootedObject proto(cx);
  if (!state.kit.ConstructProto(args, state.websocket.prototype, &proto)) return false;
  JS::RootedObject object(cx, state.kit.New(&kWebSocketClass, proto));
  JS::RootedValue value(cx);
  if (!object || !Value(cx, url, &value)) return false;
  JS::SetReservedSlot(object, WUrl, value);
  JS::SetReservedSlot(object, WReadyState, JS::Int32Value(0));
  JS::SetReservedSlot(object, WOnError, JS::NullValue());
  if (!state.events || !state.events->Attach(object)) return false;
  state.Observe("WebSocket", "constructor", int(ApiAccess::Construct));
  args.rval().setObject(*object);
  return trace.Finish(true);
}

bool WebSocketGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args, &self)) return false;
  State& state = *static_cast<State*>(WebKit::Owner(args));
  state.Observe("WebSocket", "onerror", int(ApiAccess::Read));
  args.rval().set(JS::GetReservedSlot(self, WebKit::Slot(args)));
  return JS_WrapValue(cx, args.rval());
}

bool WebSocketSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args, &self)) return false;
  State& state = *static_cast<State*>(WebKit::Owner(args));
  const int32_t slot = WebKit::Slot(args);
  JS::RootedValue old(cx, JS::GetReservedSlot(self, slot));
  JS::RootedValue value(cx, args.get(0));
  if (!(value.isObject() && JS::IsCallable(&value.toObject()))) value.setNull();
  if (old.isObject() && !Delegate(cx, self, "removeEventListener", "error", old)) return false;
  if (value.isObject() && !Delegate(cx, self, "addEventListener", "error", value)) return false;
  JS::SetReservedSlot(self, slot, value);
  state.Observe("WebSocket", "onerror", int(ApiAccess::Write));
  args.rval().setUndefined();
  return true;
}

bool WebSocketClose(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args, &self)) return false;
  State& state = *static_cast<State*>(WebKit::Owner(args));
  JS::SetReservedSlot(self, WReadyState, JS::Int32Value(3));
  state.Observe("WebSocket", "close", int(ApiAccess::Call));
  args.rval().setUndefined();
  return true;
}
}
