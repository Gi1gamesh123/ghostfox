// Per-realm SameObject ServiceWorkerRegistration / ServiceWorker instances, their
// events, the `ready` promise and the container's client message queue
// (docs/owners/SERVICE_WORKER.md). Values live in the instances' reserved slots.
#include "web_service_worker/internal.h"
#include "web_workers/clone.h"
#include "domain/browser_environment/url_value.h"
#include "interaction/realm.h"
#include "jsapi.h"
#include "js/Object.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/Promise.h"
#include "js/Realm.h"

namespace ruyijs::web_service_worker {
namespace {
// Slot numbers follow the member tables in registration.cpp; slot 0 (a method
// in both tables) holds the record id.
enum { kId = 0, kScriptUrl = 1, kState = 2 };
enum { kInstalling = 4, kWaiting = 5, kActive = 6, kPreload = 7, kScope = 8, kCache = 9 };
JSObject* Existing(State& s, uint64_t id) {
  auto found = s.objects.find(id);
  return found == s.objects.end() ? nullptr : found->second->get();
}
bool Text(JSContext* cx, JS::HandleObject object, uint32_t slot, const std::string& text) {
  JSString* value = JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(text.data(), text.size()));
  if (!value) return false;
  JS::SetReservedSlot(object, slot, JS::StringValue(value));
  return true;
}
Registration* OwnerOf(Registry& registry, uint64_t version) {
  for (auto& r : registry.list)
    for (auto* v : {&r->installing, &r->waiting, &r->active})
      if (*v && (*v)->id == version) return r.get();
  return nullptr;
}
bool FillRegistration(State& s, Registration& r, JS::HandleObject object) {
  JSContext* cx = s.cx;
  const uint32_t slots[] = {kInstalling, kWaiting, kActive};
  const std::shared_ptr<Version>* versions[] = {&r.installing, &r.waiting, &r.active};
  for (int i = 0; i < 3; ++i) {
    JS::RootedValue worker(cx, JS::NullValue());
    if (*versions[i] && !WorkerObject(s, *versions[i], &worker)) return false;
    JS::SetReservedSlot(object, slots[i], worker);
  }
  return true;
}
bool Run(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject callee(cx, &args.callee());  // before rval(), which shares the callee slot
  args.rval().setUndefined();
  auto* s = static_cast<State*>(js::GetFunctionNativeReserved(callee, 0).toPrivate());
  const uint64_t id = uint64_t(js::GetFunctionNativeReserved(callee, 1).toNumber());
  JS::RootedString type(cx, js::GetFunctionNativeReserved(callee, 2).toString());
  JS::UniqueChars name = JS_EncodeStringToUTF8(cx, type);
  if (!name || !s->registry) return !!name;
  if (std::string(name.get()) == "flush") return FlushMessages(*s);
  return FireAt(*s->registry, id, name.get());
}
}  // namespace

bool PostTask(State& s, JSNative native, uint64_t id, const char* name) {
  JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.kit.page);
  JSFunction* fn = js::NewFunctionWithReserved(cx, native ? native : Run, 0, 0, name);
  JSString* type = JS_NewStringCopyZ(cx, name);
  if (!fn || !type) return false;
  JS::RootedObject task(cx, JS_GetFunctionObject(fn));
  js::SetFunctionNativeReserved(task, 0, JS::PrivateValue(&s));
  js::SetFunctionNativeReserved(task, 1, JS::NumberValue(double(id)));
  js::SetFunctionNativeReserved(task, 2, JS::StringValue(type));
  if (s.task) return s.task(s.taskContext, task);
  JS::RootedValue ignored(cx), function(cx, JS::ObjectValue(*task));
  return JS::Call(cx, JS::UndefinedHandleValue, function, JS::HandleValueArray::empty(), &ignored);
}

bool WorkerObject(State& s, const std::shared_ptr<Version>& v, JS::MutableHandleValue result) {
  JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.kit.page);
  JS::RootedObject object(cx, Existing(s, v->id));
  if (!object) {
    object = NewObject(s, kWorkerObject);
    if (!object) return false;
    JS::SetReservedSlot(object, kId, JS::NumberValue(double(v->id)));
    if (!Text(cx, object, kScriptUrl, v->scriptURL)) return false;
    s.objects[v->id] = std::make_unique<JS::PersistentRootedObject>(cx, object);
  }
  if (!Text(cx, object, kState, v->state)) return false;
  result.setObject(*object);
  return JS_WrapValue(cx, result);
}
bool RegistrationObject(State& s, Registration& r, JS::MutableHandleValue result) {
  JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.kit.page);
  JS::RootedObject object(cx, Existing(s, r.id));
  if (!object) {
    object = NewObject(s, kRegistrationObject);
    JS::RootedObject preload(cx, NewObject(s, kPreloadObject));
    if (!object || !preload) return false;
    JS::SetReservedSlot(object, kId, JS::NumberValue(double(r.id)));
    JS::SetReservedSlot(object, kPreload, JS::ObjectValue(*preload));
    if (!Text(cx, object, kScope, r.scope) || !Text(cx, object, kCache, r.updateViaCache)) return false;
    s.objects[r.id] = std::make_unique<JS::PersistentRootedObject>(cx, object);
  }
  if (!FillRegistration(s, r, object)) return false;
  result.setObject(*object);
  return JS_WrapValue(cx, result);
}
bool UpdateVersion(Registry& registry, const Version& v) {
  for (State* s : registry.realms) {
    JS::RootedObject object(s->cx, Existing(*s, v.id));
    if (object && !Text(s->cx, object, kState, v.state)) return false;
  }
  return true;
}
bool Refresh(Registry& registry, Registration& r) {
  for (auto* v : {&r.installing, &r.waiting, &r.active})
    if (*v && !UpdateVersion(registry, **v)) return false;
  for (State* s : registry.realms) {
    JS::RootedObject object(s->cx, Existing(*s, r.id));
    JSAutoRealm realm(s->cx, s->kit.page);
    if (object && !FillRegistration(*s, r, object)) return false;
  }
  return true;
}
bool FireAt(Registry& registry, uint64_t id, const char* type) {
  for (size_t i = 0; i < registry.realms.size(); ++i) {
    State& s = *registry.realms[i];
    JS::RootedObject object(s.cx, Existing(s, id));
    if (!object) continue;
    JSAutoRealm realm(s.cx, s.kit.page);
    if (!s.events.Fire(object, type, false, false)) return false;
  }
  return true;
}
bool CheckReady(State& s) {
  if (s.readyResolved || !s.registry) return true;
  Registration* r = Match(*s.registry, ClientUrl(s), true);
  if (!r) return true;
  JSAutoRealm realm(s.cx, s.kit.page);
  JS::RootedValue value(s.cx);
  JS::RootedObject ready(s.cx, s.ready);
  s.readyResolved = true;
  return RegistrationObject(s, *r, &value) && JS::ResolvePromise(s.cx, ready, value);
}
bool DeliverMessage(State& s, uint64_t version, std::string_view clone) {
  // Firefox enables the client message queue at DOMContentLoaded, startMessages() or onmessage.
  if (!s.messagesEnabled && s.parsed && s.parsed(s.parsedContext)) s.messagesEnabled = true;
  if (!s.messagesEnabled) { s.pendingMessages.emplace_back(version, std::string(clone)); return true; }
  Registration* r = OwnerOf(*s.registry, version);
  if (!r) return true;
  JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.kit.page);
  JS::RootedValue data(cx), source(cx);
  if (!web_workers::DecodeMessage(cx, clone, &data) || !WorkerObject(s, r->active, &source)) return false;
  domain::browser_environment::UrlValue url;
  const std::string origin = domain::browser_environment::UrlValue::Resolve(r->scriptURL, "", &url) ? url.origin() : "";
  JS::RootedObject sender(cx, &source.toObject()), target(cx, s.instance);
  return s.events.FireMessage(target, data, origin.c_str(), sender);
}
bool FlushMessages(State& s) {
  s.messagesEnabled = true;
  while (!s.pendingMessages.empty()) {
    auto next = std::move(s.pendingMessages.front());
    s.pendingMessages.pop_front();
    if (!DeliverMessage(s, next.first, next.second)) return false;
  }
  return true;
}
}  // namespace ruyijs::web_service_worker
