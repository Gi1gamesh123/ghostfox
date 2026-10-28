#ifndef RUYIJS_WEB_SERVICE_WORKER_REALM_H
#define RUYIJS_WEB_SERVICE_WORKER_REALM_H
#include <memory>
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs { class PageHelpers; }
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::browser_environment { class Environment; }
namespace ruyijs::web_workers { class Realm; class Child; }
namespace ruyijs::web_service_worker {
struct State;
// ServiceWorkerContainer for one Window realm (docs/owners/SERVICE_WORKER.md):
// the interface and the SameObject navigator.serviceWorker. This runtime runs
// no service workers: there is never a controller or registration, ready stays
// pending, and register() rejects after the donor's URL checks.
class Realm final {
 public:
  // `origin`: the parent or predecessor realm whose registry this one shares.
  Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, interaction::Realm& events,
        browser_environment::Environment& environment, Realm* origin = nullptr);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  bool Install();
  // [SecureContext] navigator.serviceWorker and ServiceWorkerContainer.
  bool ConfigureExposure();
  // Window realm as a client that can run service workers: its Web Workers owner
  // runs the processes; `parsed` answers whether DOMContentLoaded was reached.
  void SetClient(web_workers::Realm* workers, bool nested, bool (*parsed)(void*), void* parsedContext);
  using TaskPort = bool (*)(void*, JS::HandleObject);
  void SetTaskPort(TaskPort port, void* context);
  // ServiceWorkerGlobalScope realm: the scope, Clients/Client/WindowClient and
  // the frame port of the worker process (scope.cpp).
  bool InstallServiceScope(web_workers::Child& child);
 private:
  std::unique_ptr<State> state_;
};
}
#endif
