#pragma once
#include "document_model/state.h"

namespace ruyijs::document_model {
// Call-scoped removal effects; original frames and focus retain their owners.
class ParentRemovalPlan final {
 public:
  bool Add(State&, NodeKey parent, NodeKey child = {});
  bool Commit();
 private:
  struct Removal { State* state; NodeKey document, focused; std::vector<NodeKey> nodes; };
  std::vector<Removal> removals_;
};
}
