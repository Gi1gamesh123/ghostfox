// Clients, Client and WindowClient (dom/webidl/Clients.webidl, Client.webidl) in a
// ServiceWorkerGlobalScope, over the registry's last client snapshot, and the
// lifecycle reply that waits for the waitUntil promises.
#include "web_service_worker/scope.h"
#include "web_workers/child.h"
#include "web_workers/clone.h"
#include "runtime/native_constructor_trace.h"
#include "jsapi.h"
#include "js/Object.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "runtime/runtime.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/ValueArray.h"
#include <cstring>

namespace ruyijs::web_service_worker {
namespace pt = protocol;
namespace {
const JSClass kClientsClass = {"Clients", 0};
// url, frameType, type, id, visibilityState, focused
const JSClass kClientClass = {"WindowClient", JSCLASS_HAS_RESERVED_SLOTS(6)};
ScopeState& Self(const JS::CallArgs& args) { return *static_cast<ScopeState*>(WebKit::Owner(args)); }
bool Illegal(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return trace.Finish(WebKit::Fail(cx, "Illegal constructor."));
}
bool Text(JSContext* cx, JS::HandleObject object, uint32_t slot, const std::string& value) {
  JSString* text = JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(value.data(), value.size()));
  if (text) JS::SetReservedSlot(object, slot, JS::StringValue(text));
  return text != nullptr;
}
bool ClientsMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  ScopeState& s = Self(args);
  State& o = s.owner;
  const int slot = WebKit::Slot(args);
  const char* names[] = {"get", "matchAll", "openWindow", "claim"};
  JS::RootedObject self(cx);
  if (!WebKit::Check(cx, args.thisv(), &kClientsClass, &self, Incompatible(names[slot], "Clients").c_str()))
    return Reject(o, args.rval());
  if (slot == 2) return RejectDom(o, "Clients.openWindow: Not allowed to open a window.", "InvalidAccessError", args.rval());
  if (slot == 3)
    return s.child.Queue(std::string(1, pt::kClaim)) && o.kit.Resolve(JS::UndefinedHandleValue, args.rval());
  if (slot == 0) {
    JS::RootedString text(cx, JS::ToString(cx, args.get(0)));
    std::string id;
    if (!text || !ToUTF8(cx, text, &id)) return Reject(o, args.rval());
    JS::RootedValue value(cx);
    for (const auto& client : s.snapshot)
      if (client.id == id && !NewClient(s, client, &value)) return false;
    return o.kit.Resolve(value, args.rval());
  }
  bool uncontrolled = false;
  std::string type = "window";
  if (args.get(0).isObject()) {
    JS::RootedObject options(cx, &args[0].toObject());
    JS::RootedValue value(cx);
    if (!JS_GetProperty(cx, options, "includeUncontrolled", &value)) return Reject(o, args.rval());
    uncontrolled = JS::ToBoolean(value);
    if (!JS_GetProperty(cx, options, "type", &value)) return Reject(o, args.rval());
    if (!value.isUndefined()) {
      JS::RootedString text(cx, JS::ToString(cx, value));
      if (!text || !ToUTF8(cx, text, &type)) return Reject(o, args.rval());
    }
  }
  JS::RootedObject list(cx, JS::NewArrayObject(cx, 0));
  if (!list) return false;
  uint32_t index = 0;
  if (type == "window" || type == "all")
    for (const auto& client : s.snapshot) {
      if (!uncontrolled && !client.controlled) continue;
      JS::RootedValue value(cx);
      if (!NewClient(s, client, &value) || !JS_SetElement(cx, list, index++, value)) return false;
    }
  JS::RootedValue value(cx, JS::ObjectValue(*list));
  return o.kit.Resolve(value, args.rval());
}
bool ClientMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  ScopeState& s = Self(args);
  const int slot = WebKit::Slot(args);
  const char* names[] = {"get url", "get frameType", "get type", "get id", "get visibilityState", "get focused",
                         "postMessage", "focus", "navigate"};
  JS::RootedObject self(cx);
  const char* iface = slot <= 3 || slot == 6 ? "Client" : "WindowClient";
  if (!WebKit::Check(cx, args.thisv(), &kClientClass, &self, Incompatible(names[slot], iface).c_str()))
    return slot >= 7 ? Reject(s.owner, args.rval()) : false;
  if (slot <= 5) { args.rval().set(JS::GetReservedSlot(self, slot)); return true; }
  if (slot == 7) return RejectDom(s.owner, "WindowClient.focus: Not allowed to focus a window.", "InvalidAccessError", args.rval());
  if (slot == 8) {
    (void)WebKit::Fail(cx, "WindowClient.navigate: Client is not controlled by this ServiceWorker.");
    return Reject(s.owner, args.rval());
  }
  if (!args.length()) return WebKit::Fail(cx, "Client.postMessage: At least 1 argument required, but only 0 passed");
  JS::RootedObject exception(cx, s.child.exceptionPrototype());
  std::string clone, id;
  JS::RootedString text(cx, JS::GetReservedSlot(self, 3).toString());
  if (!ToUTF8(cx, text, &id) || !web_workers::EncodeMessage(cx, exception, args[0], args.get(1), &clone)) return false;
  args.rval().setUndefined();
  return s.child.Queue(pt::Writer(pt::kClientMessage).Field(id).Take(clone));
}
bool Settled(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject callee(cx, &args.callee());  // before rval(), which shares the callee slot
  args.rval().setUndefined();
  auto* s = static_cast<ScopeState*>(js::GetFunctionNativeReserved(callee, 0).toPrivate());
  JS::RootedString name(cx, js::GetFunctionNativeReserved(callee, 1).toString());
  std::string text;
  if (!ToUTF8(cx, name, &text)) return false;
  return s->child.Queue(pt::Writer(pt::kLifecycleDone).Field(text).Field(js::GetFunctionNativeReserved(callee, 2).toBoolean() ? "1" : "0").Take());
}
JSObject* Reaction(ScopeState& s, const std::string& name, bool ok) {
  JSContext* cx = s.owner.cx;
  JSFunction* fn = js::NewFunctionWithReserved(cx, Settled, 1, 0, "waitUntil settled");
  JSString* text = JS_NewStringCopyN(cx, name.data(), name.size());
  if (!fn || !text) return nullptr;
  JSObject* object = JS_GetFunctionObject(fn);
  js::SetFunctionNativeReserved(object, 0, JS::PrivateValue(&s));
  js::SetFunctionNativeReserved(object, 1, JS::StringValue(text));
  js::SetFunctionNativeReserved(object, 2, JS::BooleanValue(ok));
  return object;
}
}  // namespace

bool NewClient(ScopeState& s, const pt::Client& c, JS::MutableHandleValue result) {
  JSContext* cx = s.owner.cx;
  JS::RootedObject prototype(cx, s.windowClient.prototype);
  JS::RootedObject object(cx, s.kit.New(&kClientClass, prototype));
  if (!object || !Text(cx, object, 0, c.url) || !Text(cx, object, 1, c.frameType) || !Text(cx, object, 2, "window") ||
      !Text(cx, object, 3, c.id) || !Text(cx, object, 4, c.visibility)) return false;
  JS::SetReservedSlot(object, 5, JS::BooleanValue(c.focused));
  result.setObject(*object);
  return true;
}
bool ReplyWhenSettled(ScopeState& s, JS::HandleObject lifetimes, const std::string& name) {
  JSContext* cx = s.owner.cx;
  uint32_t length = 0;
  if (lifetimes && !JS::GetArrayLength(cx, lifetimes, &length)) return false;
  if (!length) return s.child.Queue(pt::Writer(pt::kLifecycleDone).Field(name).Field("1").Take());
  JS::RootedObjectVector promises(cx);
  for (uint32_t i = 0; i < length; ++i) {
    JS::RootedValue value(cx);
    if (!JS_GetElement(cx, lifetimes, i, &value) || !value.isObject() || !promises.append(&value.toObject())) return false;
  }
  JS::RootedObject all(cx, JS::GetWaitForAllPromise(cx, promises));
  JS::RootedObject fulfilled(cx, Reaction(s, name, true)), rejected(cx, Reaction(s, name, false));
  return all && fulfilled && rejected && JS::AddPromiseReactions(cx, all, fulfilled, rejected);
}
bool InstallClients(ScopeState& s) {
  JSContext* cx = s.owner.cx;
  JS::RootedObject object(cx, JS::GetRealmObjectPrototype(cx));
  if (!s.kit.Define("Clients", &kClientsClass, object, Illegal, &s.clients) ||
      !s.kit.Define("Client", &kClientClass, object, Illegal, &s.client)) return false;
  const char* clients[] = {"get", "matchAll", "openWindow", "claim"};
  const unsigned lengths[] = {1, 0, 1, 0};
  for (int i = 0; i < 4; ++i)
    if (!s.kit.Method(s.clients, "Clients", clients[i], ClientsMember, lengths[i], i)) return false;
  if (!s.kit.Method(s.client, "Client", "postMessage", ClientMember, 1, 6)) return false;
  const char* attributes[] = {"url", "frameType", "type", "id"};
  for (int i = 0; i < 4; ++i)
    if (!s.kit.Accessor(s.client, "Client", attributes[i], ClientMember, nullptr, i)) return false;
  JS::RootedObject clientPrototype(cx, s.client.prototype);
  if (!s.kit.Define("WindowClient", &kClientClass, clientPrototype, Illegal, &s.windowClient)) return false;
  JS::RootedObject windowCtor(cx, s.windowClient.constructor), clientCtor(cx, s.client.constructor);
  if (!JS_SetPrototype(cx, windowCtor, clientCtor) ||
      !s.kit.Method(s.windowClient, "WindowClient", "focus", ClientMember, 0, 7) ||
      !s.kit.Method(s.windowClient, "WindowClient", "navigate", ClientMember, 1, 8) ||
      !s.kit.Accessor(s.windowClient, "WindowClient", "visibilityState", ClientMember, nullptr, 4) ||
      !s.kit.Accessor(s.windowClient, "WindowClient", "focused", ClientMember, nullptr, 5)) return false;
  JS::RootedObject clientsPrototype(cx, s.clients.prototype);
  s.clientsObject = s.kit.New(&kClientsClass, clientsPrototype);
  return s.clientsObject != nullptr;
}
}  // namespace ruyijs::web_service_worker
