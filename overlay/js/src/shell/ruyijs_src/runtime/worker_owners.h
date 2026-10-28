// Worker realm owners added after graphics (docs/owners/WORKER.md, 2026-10-21):
// IndexedDB, WebGPU, Permissions and Web Locks from their one production
// installer each, plus the Resource Network owner's XHR/WebSocket, each bound
// to the Worker's one Scheduling task queue. The Worker is its own process, so
// IndexedDB stores and lock queues are local to it (recorded debt: Firefox
// shares them per origin with the creating document).
#ifndef RUYIJS_RUNTIME_WORKER_OWNERS_H
#define RUYIJS_RUNTIME_WORKER_OWNERS_H
#include <memory>
#include "js/RootingAPI.h"

struct JSContext;
namespace ruyijs { class PageHelpers; }
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::browser_environment { class Environment; }
namespace ruyijs::execution_scheduling { class Realm; }
namespace ruyijs::resource_network { class Realm; }
namespace ruyijs::web_workers { class Child; }

namespace ruyijs {
class WorkerOwners {
 public:
  WorkerOwners(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, interaction::Realm& events,
               browser_environment::Environment& environment, resource_network::Realm& resources,
               execution_scheduling::Realm& scheduling, web_workers::Child& child,
               void (*checkpoint)(JSContext*));
  ~WorkerOwners();
  WorkerOwners(const WorkerOwners&) = delete;
  WorkerOwners& operator=(const WorkerOwners&) = delete;
  // After Environment::InstallWorker (WorkerNavigator exists) and the network.
  bool Install(JS::HandleObject globalScopePrototype);
  // Last step of the Worker assembly: Scheduling's WorkerGlobalScope members
  // onto their prototype, then WorkerGlobalScope/WorkerNavigator key order.
  bool Finish(JS::HandleObject globalScopePrototype);

 private:
  struct Parts;
  std::unique_ptr<Parts> parts_;
};
}  // namespace ruyijs
#endif
