// Synchronous native identity publication from the unique DOM owner.
#ifndef RUYIJS_DOCUMENT_NODE_LIFECYCLE_H
#define RUYIJS_DOCUMENT_NODE_LIFECYCLE_H
#include "document_model/model.h"

namespace ruyijs::document_model {
class Realm;
enum class NodeLifecycleKind { Rekeyed, Released, Retired, Transferred };
struct NodeLifecycleNotice {
  NodeLifecycleKind kind;
  // Borrowed only for this callback. Rekeyed uses the original commit pairs;
  // Released uses the original subtree's keys after successful reclamation.
  // Retired invalidates every remaining key of this owner, with empty views.
  const std::vector<std::pair<NodeKey, NodeKey>>& rekeyed;
  const std::vector<NodeKey>& released;
  Realm* destination = nullptr;  // Transferred only; original target data owner.
};
// Consumers may update their own native records or unbind this port. They must
// not retain the views, execute JS, collect garbage, or reenter DOM operations.
using NodeLifecycleSink = void (*)(void*, const NodeLifecycleNotice&) noexcept;
}
#endif
