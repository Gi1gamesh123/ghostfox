#ifndef RUYIJS_WEB_BROADCAST_REALM_H
#define RUYIJS_WEB_BROADCAST_REALM_H
#include <memory>
#include <string>
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs { class PageHelpers; }
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::browser_environment { class Environment; }
namespace ruyijs::web_broadcast {
struct State;
struct Registry;
// BroadcastChannel for one Window realm (docs/owners/BROADCAST_CHANNEL.md). The
// main context owns the Context's one channel registry; an iframe realm uses its
// parent's, so same-origin channels in every frame of the Context meet there.
class Realm final {
 public:
  Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, interaction::Realm& events,
        browser_environment::Environment& environment, Realm* parent);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  bool Install();
  // Scheduling port: queues one delivery task (a function) on the Context's
  // one macrotask queue. Bound by the runtime coordinator with the window
  // message ports.
  using TaskPort = bool (*)(void* context, JS::HandleObject task);
  void SetTaskPort(TaskPort port, void* context);
  // Cross-process delivery (remote.cpp). The relay port receives every message
  // posted in this registry (in the sender's realm); Receive delivers a message
  // relayed from another process to the open channels of that origin and name.
  using RelayPort = bool (*)(void* context, const std::string& origin, const std::u16string& name,
                             JS::HandleValue data);
  void SetRelayPort(RelayPort port, void* context);
  void ClearRelayPort(void* context);  // only when `context` is the bound relay
  bool Receive(const std::string& origin, const std::u16string& name, JS::HandleValue data);
 private:
  std::unique_ptr<State> state_;
};
}
#endif
