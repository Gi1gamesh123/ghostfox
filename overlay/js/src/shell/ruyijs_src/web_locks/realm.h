#ifndef RUYIJS_WEB_LOCKS_REALM_H
#define RUYIJS_WEB_LOCKS_REALM_H
#include <memory>
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs { class PageHelpers; }
namespace ruyijs::browser_environment { class Environment; }
namespace ruyijs::web_locks {
struct State;
struct Registry;
// Web Locks for one Window realm (docs/owners/WEB_LOCKS.md): the LockManager and
// Lock interfaces and the SameObject navigator.locks. The main context owns the
// Context's one lock registry (one domain LockQueue per origin, like the donor's
// per-principal ManagedLocks); an iframe realm uses its parent's, so same-origin
// frames contend for the same names.
class Realm final {
 public:
  Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
        browser_environment::Environment& environment, Realm* parent);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  // Installs after Document Model (DOMException) and before the Navigator key
  // order is fixed.
  bool Install();
  // [SecureContext]: navigator.locks, LockManager and Lock exist only while the
  // realm's document is a secure context (runtime/secure_exposure.cpp).
  bool ConfigureExposure();
  // Scheduling port: grants run their callback as one task, as the donor's
  // LockRequestChild::RecvResolve runs on an IPC task.
  using TaskPort = bool (*)(void* context, JS::HandleObject task);
  void SetTaskPort(TaskPort port, void* context);
 private:
  std::unique_ptr<State> state_;
};
}
#endif
