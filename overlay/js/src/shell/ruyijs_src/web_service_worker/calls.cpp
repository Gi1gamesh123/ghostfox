// Methods of ServiceWorker, ServiceWorkerRegistration and NavigationPreloadManager
// instances (docs/owners/SERVICE_WORKER.md). Instances exist only once a registry
// created them, so every receiver here carries its record id in slot 0.
#include "web_service_worker/internal.h"
#include "web_service_worker/protocol.h"
#include "web_workers/clone.h"
#include "browser_environment/environment.h"
#include "runtime/runtime.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include <cstring>

namespace ruyijs::web_service_worker {
namespace pt = protocol;
namespace {
uint64_t IdOf(JS::HandleObject self) {
  const JS::Value id = JS::GetReservedSlot(self, 0);
  return id.isNumber() ? uint64_t(id.toNumber()) : 0;
}
bool Resolved(State& s, JS::HandleValue value, const JS::CallArgs& args) { return s.kit.Resolve(value, args.rval()); }
bool PostMessage(State& s, JS::HandleObject self, const JS::CallArgs& args) {
  JSContext* cx = s.cx;
  if (!args.length())
    return WebKit::Fail(cx, "ServiceWorker.postMessage: At least 1 argument required, but only 0 passed");
  const uint64_t id = IdOf(self);
  Registration* owner = nullptr;
  if (s.registry)
    for (auto& r : s.registry->list)
      for (auto* v : {&r->installing, &r->waiting, &r->active})
        if (*v && (*v)->id == id) owner = r.get();
  // A redundant worker drops the message silently (Firefox).
  if (!owner || owner->removed) { args.rval().setUndefined(); return true; }
  JS::RootedValue prototype(cx);
  JS::RootedObject constructor(cx, s.exception);
  if (!JS_GetProperty(cx, constructor, "prototype", &prototype) || !prototype.isObject()) return false;
  JS::RootedObject exception(cx, &prototype.toObject());
  std::string clone;
  if (!web_workers::EncodeMessage(cx, exception, args[0], args.get(1), &clone)) return false;
  JS::RootedValue origin(cx);
  std::string originText;
  if (s.environment.Origin(&origin) && origin.isString()) {
    JS::RootedString text(cx, origin.toString());
    if (!ToUTF8(cx, text, &originText)) return false;
  }
  pt::Writer frame(pt::kMessage);
  frame.Record(ClientRecord(s)).Field(originText);
  if (!SendFrame(*owner, frame.Take(clone))) return WebKit::Fail(cx, "ServiceWorker message queue is full");
  args.rval().setUndefined();
  return true;
}
bool RegistrationCall(State& s, const char* member, JS::HandleObject self, const JS::CallArgs& args) {
  JSContext* cx = s.cx;
  Registration* r = s.registry ? Find(*s.registry, IdOf(self)) : nullptr;
  if (!std::strcmp(member, "showNotification")) {
    (void)WebKit::Fail(cx, "ServiceWorkerRegistration.showNotification: Permission to show Notification denied.");
    return Reject(s, args.rval());
  }
  if (!std::strcmp(member, "getNotifications")) {
    JS::RootedObject empty(cx, JS::NewArrayObject(cx, 0));
    JS::RootedValue value(cx, JS::ObjectOrNullValue(empty));
    return empty && Resolved(s, value, args);
  }
  if (!std::strcmp(member, "unregister")) {
    JS::RootedValue result(cx, JS::BooleanValue(r && !r->removed));
    if (r && !r->removed) {
      std::vector<uint64_t> versions;
      for (auto* v : {&r->installing, &r->waiting, &r->active}) if (*v) versions.push_back((*v)->id);
      Remove(*s.registry, *r);
      for (uint64_t id : versions) if (!PostTask(s, nullptr, id, "statechange")) return false;
    }
    return Resolved(s, result, args);
  }
  // update(): no update check runs here; the registration is current.
  if (!r || r->removed)
    return RejectDom(s, "ServiceWorkerRegistration.update: Failed to update the ServiceWorker: the registration has been uninstalled.",
                     "InvalidStateError", args.rval());
  JS::RootedValue value(cx);
  return RegistrationObject(s, *r, &value) && Resolved(s, value, args);
}
bool PreloadCall(State& s, const char* member, const JS::CallArgs& args) {
  JSContext* cx = s.cx;
  if (std::strcmp(member, "getState")) return Resolved(s, JS::UndefinedHandleValue, args);
  JS::RootedObject state(cx, JS_NewPlainObject(cx));
  JS::RootedString header(cx, JS_NewStringCopyZ(cx, "true"));
  if (!state || !header || !JS_DefineProperty(cx, state, "enabled", JS::FalseHandleValue, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, state, "headerValue", header, JSPROP_ENUMERATE)) return false;
  JS::RootedValue value(cx, JS::ObjectValue(*state));
  return Resolved(s, value, args);
}
}  // namespace

bool CallMember(State& s, ObjectKind kind, const char* member, JS::HandleObject self, const JS::CallArgs& args) {
  if (kind == kWorkerObject) return PostMessage(s, self, args);
  if (kind == kRegistrationObject) return RegistrationCall(s, member, self, args);
  return PreloadCall(s, member, args);
}
}  // namespace ruyijs::web_service_worker
