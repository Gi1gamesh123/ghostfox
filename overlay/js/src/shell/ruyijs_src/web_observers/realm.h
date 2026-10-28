#ifndef RUYIJS_WEB_OBSERVERS_REALM_H
#define RUYIJS_WEB_OBSERVERS_REALM_H
#include <memory>
#include "js/RootingAPI.h"
struct JSContext;
namespace ruyijs { class PageHelpers; }
namespace ruyijs::document_model { class Realm; }
namespace ruyijs::web_observers {
struct State;
// MutationObserver owner. Records come from the Document Model's real-change
// log (one notice per applied change); this owner holds every policy decision:
// option parsing, attribute filters, subtree matching, batching and delivery.
// Delivery runs on the Context's single microtask queue, bound as a port after
// the Scheduling owner exists, so the interface can install before it.
class Realm final {
 public:
  // The Scheduling owner's microtask queue. Bound by the runtime coordinator
  // once that owner exists; a realm without it queues no delivery at all.
  using MicrotaskPort = bool (*)(void* context, JS::HandleObject callback);
  // The Scheduling owner's single clock, in milliseconds. Bound next to the
  // microtask port; an unbound realm stamps entries with zero.
  using ClockPort = double (*)(void* context);
  Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
        document_model::Realm& document);
  // A Worker realm: no document, only the ReportingObserver interfaces.
  Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers);
  bool InstallWorker();
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  bool Install();
  void SetMicrotaskPort(MicrotaskPort port, void* context);
  void SetClockPort(ClockPort port, void* context);
  // An iframe shares the agent's microtask queue and clock with the frame that
  // created it; its own constructors and prototypes still come from its own
  // Install.
  void CopyPortsFrom(const Realm& other);
 private:
  std::unique_ptr<State> state_;
};
}
#endif
