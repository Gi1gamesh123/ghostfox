// ServiceWorkerContainer attributes: controller (always null), the SameObject
// ready promise (never settles) and the three EventHandler attributes.
#include "web_service_worker/internal.h"
#include "interaction/realm.h"
#include "jsapi.h"
#include "js/CallAndConstruct.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::web_service_worker {
// Enables the client message queue; queued messages dispatch in a later task.
static bool Start(State& s) {
  if (s.messagesEnabled) return true;
  s.messagesEnabled = true;
  return s.pendingMessages.empty() || PostTask(s, nullptr, 0, "flush");
}
bool StartMessages(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  if (!Container(cx, args, "startMessages")) return false;
  args.rval().setUndefined();
  return Start(s);
}
bool Controller(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  if (!Container(cx, args, "get controller")) return false;
  Registration* r = s.controller && s.registry ? Find(*s.registry, s.controller) : nullptr;
  if (!r || !r->active) { args.rval().setNull(); return true; }
  return WorkerObject(s, r->active, args.rval());
}
bool Ready(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  if (!Container(cx, args, "get ready") || !CheckReady(s)) return false;
  args.rval().setObject(*s.ready);
  return JS_WrapValue(cx, args.rval());
}
bool HandlerGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int which = WebKit::Slot(args);
  const std::string member = std::string("get on") + kHandlers[which];
  JSObject* self = Container(cx, args, member.c_str());
  if (!self) return false;
  args.rval().set(JS::GetReservedSlot(self, which));
  return JS_WrapValue(cx, args.rval());
}
bool HandlerSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  const int which = WebKit::Slot(args);
  const std::string member = std::string("set on") + kHandlers[which];
  JS::RootedObject self(cx, Container(cx, args, member.c_str()));
  if (!self) return false;
  JS::RootedValue previous(cx, JS::GetReservedSlot(self, which)), desired(cx, args.get(0));
  if (!desired.isObject() || !JS::IsCallable(&desired.toObject())) desired.setNull();
  JSAutoRealm realm(cx, s.kit.page);
  if (!JS_WrapValue(cx, &desired) || !JS_WrapValue(cx, &previous)) return false;
  JS::RootedObject prototype(cx, s.events.prototype());
  JS::RootedValue function(cx), receiver(cx, JS::ObjectValue(*self)), ignored(cx);
  JS::RootedString type(cx, JS_NewStringCopyZ(cx, kHandlers[which]));
  if (!type) return false;
  JS::RootedValueArray<2> parameters(cx);
  parameters[0].setString(type);
  for (int operation = 0; operation < 2; ++operation) {
    parameters[1].set(operation ? desired : previous);
    if (!parameters[1].isObject()) continue;
    if (!JS_GetProperty(cx, prototype, operation ? "addEventListener" : "removeEventListener", &function) ||
        !JS::Call(cx, receiver, function, parameters, &ignored)) return false;
  }
  JS::SetReservedSlot(self, which, desired);
  args.rval().setUndefined();
  // Setting onmessage starts the client message queue (Firefox ServiceWorkerContainer.h).
  return which != SlotMessage || !desired.isObject() || Start(s);
}
}  // namespace ruyijs::web_service_worker
