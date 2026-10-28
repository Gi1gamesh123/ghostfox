// Register/install/activate jobs of the Window registry and the frames a service
// worker process sends back (docs/owners/SERVICE_WORKER.md). One job runs per
// registration; frames arrive one per Web Workers poll task.
#include "web_service_worker/internal.h"
#include "web_service_worker/protocol.h"
#include "web_workers/realm.h"
#include "browser_environment/environment.h"
#include "interaction/realm.h"
#include "runtime/runtime.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/Realm.h"
#include <algorithm>

namespace ruyijs::web_service_worker {
namespace pt = protocol;
Registration* Match(Registry& registry, std::string_view url, bool active) {
  Registration* best = nullptr;
  for (auto& entry : registry.list) {
    if (entry->removed || (active && !entry->active) || url.substr(0, entry->scope.size()) != entry->scope) continue;
    if (!best || entry->scope.size() > best->scope.size()) best = entry.get();
  }
  return best;
}
Registration* Find(Registry& registry, uint64_t id) {
  for (auto& entry : registry.list) if (entry->id == id) return entry.get();
  return nullptr;
}
std::string ClientUrl(State& s) {
  JS::RootedValue href(s.cx);
  std::string text;
  if (s.environment.URL(&href) && href.isString()) {
    JS::RootedString value(s.cx, href.toString());
    if (!ToUTF8(s.cx, value, &text)) JS_ClearPendingException(s.cx);
  }
  return text;
}
pt::Client ClientRecord(State& s) {
  return {s.clientId, ClientUrl(s), s.nested ? "nested" : "top-level", "visible", !s.nested, s.controller != 0};
}
bool SendFrame(Registration& r, std::string frame) {
  return r.home && r.home->workers && r.process && r.home->workers->QueueService(r.process, std::move(frame));
}
namespace {
bool SendClients(Registry& registry, Registration& r) {
  pt::Writer frame(pt::kClients);
  frame.Field(std::to_string(registry.realms.size()));
  for (State* realm : registry.realms) frame.Record(ClientRecord(*realm));
  pt::Writer state(pt::kRegistration);
  state.Field(r.scope).Field(r.scriptURL).Field(r.updateViaCache);
  for (const auto* v : {&r.installing, &r.waiting, &r.active}) state.Field(*v ? (*v)->state : "");
  return SendFrame(r, frame.Take()) && SendFrame(r, state.Take());
}
// Settles the register() promises of the home realm for this registration.
bool Settle(State& s, Registration& r, const char* error) {
  JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.kit.page);
  for (size_t i = 0; i < s.pendingRegisters.size();) {
    if (s.pendingRegisters[i].first != r.id) { ++i; continue; }
    JS::RootedObject promise(cx, *s.pendingRegisters[i].second);
    s.pendingRegisters.erase(s.pendingRegisters.begin() + i);
    JS::RootedValue value(cx);
    if (!error) {
      if (!RegistrationObject(s, r, &value) || !JS::ResolvePromise(cx, promise, value)) return false;
      continue;
    }
    (void)WebKit::Fail(cx, error);
    if (!JS_GetPendingException(cx, &value)) return false;
    JS_ClearPendingException(cx);
    if (!JS::RejectPromise(cx, promise, value)) return false;
  }
  return true;
}
bool SetState(Registry& registry, Registration& r, const std::shared_ptr<Version>& v, const char* state) {
  v->state = state;
  return Refresh(registry, r) && FireAt(registry, v->id, "statechange");
}
bool Fail(State& s, Registration& r, const std::string& message) {
  Registry& registry = *s.registry;
  if (!Settle(s, r, message.c_str())) return false;
  std::shared_ptr<Version> v = r.installing;
  Remove(registry, r);
  return !v || FireAt(registry, v->id, "statechange");
}
}  // namespace

void Remove(Registry& registry, Registration& r) {
  r.removed = true;
  for (auto* slot : {&r.installing, &r.waiting, &r.active})
    if (*slot) { (*slot)->state = "redundant"; (void)UpdateVersion(registry, **slot); }
  r.installing = r.waiting = r.active = nullptr;
  if (r.home && r.home->workers && r.process) r.home->workers->StopService(r.process);
  r.process = 0;
  (void)Refresh(registry, r);
}

bool StartRegister(State& s, const std::string& scope, const std::string& script, const std::string& cache,
                   JS::MutableHandleValue promise) {
  JSContext* cx = s.cx;
  Registry& registry = *s.registry;
  JS::RootedObject created(cx, JS::NewPromiseObject(cx, nullptr));
  if (!created) return false;
  promise.setObject(*created);
  for (auto& entry : registry.list) {
    if (entry->removed || entry->scope != scope) continue;
    if (entry->scriptURL == script && (entry->active || entry->waiting)) {  // nothing to update
      JS::RootedValue value(cx);
      return RegistrationObject(s, *entry, &value) && JS::ResolvePromise(cx, created, value);
    }
    if (entry->scriptURL == script && entry->home == &s) {  // the same job is running
      s.pendingRegisters.emplace_back(entry->id, std::make_unique<JS::PersistentRootedObject>(cx, created));
      return true;
    }
    Remove(registry, *entry);  // a different script replaces the registration (no update job)
  }
  auto r = std::make_shared<Registration>();
  r->id = ++registry.next;
  r->scope = scope;
  r->scriptURL = script;
  r->updateViaCache = cache;
  r->home = &s;
  registry.list.push_back(r);
  s.pendingRegisters.emplace_back(r->id, std::make_unique<JS::PersistentRootedObject>(cx, created));
  if (!s.workers->StartService(script, &ProcessFrame, &s, &r->process)) return false;
  return true;
}

bool ProcessFrame(void* context, uint64_t process, std::string_view frame) {
  State& s = *static_cast<State*>(context);
  Registry& registry = *s.registry;
  Registration* found = nullptr;
  for (auto& entry : registry.list) if (entry->home == &s && entry->process == process) found = entry.get();
  if (!found) return true;
  Registration& r = *found;
  JSAutoRealm realm(s.cx, s.kit.page);
  const char tag = frame.empty() ? '\0' : frame[0];
  if (tag == '\x04' || tag == '\0') {  // load or evaluation failure, or the process exited early
    if (r.active || r.waiting) { r.process = 0; return true; }
    return Fail(s, r, "ServiceWorker script at " + r.scriptURL + " for scope " + r.scope +
                          " encountered an error during installation.");
  }
  if (tag == pt::kEvaluationFailed && !r.active && !r.waiting)
    return Fail(s, r, "ServiceWorker script at " + r.scriptURL + " for scope " + r.scope +
                          " threw an exception during script evaluation.");
  pt::Reader reader(frame);
  if (tag == pt::kEvaluated) {
    auto v = std::make_shared<Version>();
    v->id = ++registry.next;
    v->scriptURL = r.scriptURL;
    v->state = "installing";
    r.installing = v;
    if (!Refresh(registry, r) || !Settle(s, r, nullptr) || !SendClients(registry, r)) return false;
    if (!PostTask(s, nullptr, r.id, "updatefound")) return false;
    return SendFrame(r, pt::Writer(pt::kLifecycle).Field("install").Take());
  }
  if (tag == pt::kLifecycleDone) {
    std::string name, ok;
    if (!reader.Field(&name) || !reader.Field(&ok)) return true;
    if (name == "install" && r.installing) {
      std::shared_ptr<Version> v = r.installing;
      if (ok != "1") {
        Remove(registry, r);
        return FireAt(registry, v->id, "statechange");
      }
      r.installing = nullptr;
      r.waiting = v;
      if (!SetState(registry, r, v, "installed")) return false;
      r.waiting = nullptr;  // no active worker to wait for: activate now
      r.active = v;
      if (!SetState(registry, r, v, "activating") || !SendClients(registry, r)) return false;
      for (State* realm : registry.realms) if (!CheckReady(*realm)) return false;
      return SendFrame(r, pt::Writer(pt::kLifecycle).Field("activate").Take());
    }
    if (name == "activate" && r.active) return SetState(registry, r, r.active, "activated");
    return true;
  }
  if (tag == pt::kClientMessage) {
    std::string client;
    if (!reader.Field(&client) || !r.active) return true;
    for (State* realm : registry.realms)
      if (realm->clientId == client) return DeliverMessage(*realm, r.active->id, reader.Rest());
    return true;
  }
  if (tag == pt::kClaim && r.active) {
    for (State* realm : registry.realms) {
      if (Match(registry, ClientUrl(*realm), true) != &r || realm->controller == r.id) continue;
      realm->controller = r.id;
      JSAutoRealm inner(realm->cx, realm->kit.page);
      if (!realm->events.Fire(realm->instance, "controllerchange", false, false)) return false;
    }
    return SendClients(registry, r);
  }
  return true;
}
}  // namespace ruyijs::web_service_worker
