// BroadcastChannel between the processes of one Context (docs/owners/BROADCAST_CHANNEL.md):
// the runtime encodes a posted message as a '\x07' frame (origin, name, structured clone)
// and routes it Window -> every worker process, worker -> its parent, its own children
// and its realm, parent -> child -> grandchild. Each hop delivers to its own realm once.
#ifndef RUYIJS_RUNTIME_BROADCAST_PORTS_H
#define RUYIJS_RUNTIME_BROADCAST_PORTS_H
#include "js/RootingAPI.h"
namespace ruyijs::web_broadcast { class Realm; }
namespace ruyijs::web_workers { class Realm; class Child; }
namespace ruyijs {
struct BroadcastRoute {
  JSContext* cx;
  JS::PersistentRootedObject page;   // the realm that encodes and decodes
  web_broadcast::Realm& channels;    // this process's channel registry
  web_workers::Realm* children;      // worker processes this realm runs (or null)
  web_workers::Child* parent;        // the parent process link of a worker (or null)
  BroadcastRoute(JSContext* cx, JS::HandleObject page, web_broadcast::Realm& channels,
                 web_workers::Realm* children, web_workers::Child* parent)
      : cx(cx), page(cx, page), channels(channels), children(children), parent(parent) {}
};
// Binds the registry's relay port, the children's broadcast sink and the parent's
// broadcast port to `route`; Unbind clears them before the owners are destroyed.
void BindBroadcastRoute(BroadcastRoute& route);
void UnbindBroadcastRoute(BroadcastRoute& route);
}  // namespace ruyijs
#endif
