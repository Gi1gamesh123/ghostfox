// Service worker registrations of one origin chain (docs/owners/SERVICE_WORKER.md,
// ARCHITECTURE.md "Service Worker execution"). Plain records: the JS objects that
// represent them are per realm (objects.cpp), the processes belong to Web Workers.
#ifndef RUYIJS_WEB_SERVICE_WORKER_REGISTRY_H
#define RUYIJS_WEB_SERVICE_WORKER_REGISTRY_H
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "js/TypeDecls.h"
#include "web_service_worker/protocol.h"

namespace ruyijs::web_service_worker {
struct State;
// One worker version: "parsed", "installing", "installed", "activating", "activated", "redundant".
struct Version {
  uint64_t id = 0;
  std::string scriptURL, state = "parsed";
};
struct Registration {
  uint64_t id = 0;
  std::string scope, scriptURL, updateViaCache = "imports";
  std::shared_ptr<Version> installing, waiting, active;
  State* home = nullptr;     // the realm whose Web Workers owner runs the process
  uint64_t process = 0;      // Web Workers service process id, 0 when stopped
  bool removed = false;      // unregistered or failed
};
struct Registry {
  uint64_t next = 0;
  std::vector<std::shared_ptr<Registration>> list;
  std::vector<State*> realms;  // every Window realm of the chain (the clients)
};

// The registration whose scope is the longest prefix of `url`, with an active
// worker when `active` is set (Match Service Worker Registration).
Registration* Match(Registry&, std::string_view url, bool active);
Registration* Find(Registry&, uint64_t id);
// The SW frame sink for the home realm (registry.cpp).
bool ProcessFrame(void* state, uint64_t process, std::string_view frame);
// Starts the update/install job for a validated register() call.
bool StartRegister(State&, const std::string& scope, const std::string& script,
                   const std::string& updateViaCache, JS::MutableHandleValue promise);
// Marks the registration removed, its workers redundant, and stops the process.
void Remove(Registry&, Registration&);
// Per realm objects and events (objects.cpp).
bool RegistrationObject(State&, Registration&, JS::MutableHandleValue);
bool WorkerObject(State&, const std::shared_ptr<Version>&, JS::MutableHandleValue);
bool Refresh(Registry&, Registration&);                       // slots of every realm's objects
bool UpdateVersion(Registry&, const Version&);                 // state slot of one worker's objects
std::string ClientUrl(State&);                                 // the client's current document URL
protocol::Client ClientRecord(State&);                         // this realm as a WindowClient
bool SendFrame(Registration&, std::string frame);              // to the registration's process
bool FireAt(Registry&, uint64_t id, const char* type);        // statechange / updatefound
bool CheckReady(State&);                                       // resolves `ready` when in scope
bool DeliverMessage(State&, uint64_t version, std::string_view clone);
bool FlushMessages(State&);
}  // namespace ruyijs::web_service_worker
#endif
