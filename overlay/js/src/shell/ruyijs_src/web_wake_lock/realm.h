#ifndef RUYIJS_WEB_WAKE_LOCK_REALM_H
#define RUYIJS_WEB_WAKE_LOCK_REALM_H
#include <memory>
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs { class PageHelpers; }
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::browser_environment { class Environment; }
namespace ruyijs::web_wake_lock {
struct State;
// Screen Wake Lock for one Window realm (docs/owners/WAKE_LOCK.md): WakeLock,
// WakeLockSentinel and the SameObject navigator.wakeLock. Realm-local; this
// headless runtime has no display to keep awake, so a granted sentinel only
// records its released state.
class Realm final {
 public:
  Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, interaction::Realm& events,
        browser_environment::Environment& environment);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  bool Install();
  // Scheduling port: the obtain step runs as one queued task (WakeLockJS::Request).
  using TaskPort = bool (*)(void* context, JS::HandleObject task);
  void SetTaskPort(TaskPort port, void* context);
  // [SecureContext] navigator.wakeLock, WakeLock and WakeLockSentinel.
  bool ConfigureExposure();
 private:
  std::unique_ptr<State> state_;
};
}
#endif
