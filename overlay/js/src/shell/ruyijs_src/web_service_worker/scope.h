// The ServiceWorkerGlobalScope side of the Service Worker owner (docs/owners/
// SERVICE_WORKER.md): scope members, Clients/Client/WindowClient, and the frames of
// the Window registry that runs this worker process.
#ifndef RUYIJS_WEB_SERVICE_WORKER_SCOPE_H
#define RUYIJS_WEB_SERVICE_WORKER_SCOPE_H
#include <string>
#include <string_view>
#include <vector>
#include "web_service_worker/internal.h"
#include "web_service_worker/protocol.h"

namespace ruyijs::web_workers { class Child; }
namespace ruyijs::web_service_worker {
struct ScopeState {
  ScopeState(State& owner, web_workers::Child& child)
      : owner(owner), child(child), kit(owner.cx, owner.kit.page, *owner.kit.helpers, this),
        scope(owner.cx), clients(owner.cx), client(owner.cx), windowClient(owner.cx),
        clientsObject(owner.cx), registration(owner.cx), self(owner.cx), handlers(owner.cx) {}
  State& owner;
  web_workers::Child& child;
  WebKit kit;
  WebFace scope, clients, client, windowClient;
  JS::PersistentRootedObject clientsObject, registration, self, handlers;
  std::vector<protocol::Client> snapshot;
};
// scope_clients.cpp
bool InstallClients(ScopeState&);
bool NewClient(ScopeState&, const protocol::Client&, JS::MutableHandleValue);
// The lifecycle reply once every waitUntil promise of `lifetimes` settled.
bool ReplyWhenSettled(ScopeState&, JS::HandleObject lifetimes, const std::string& name);
}  // namespace ruyijs::web_service_worker
#endif
