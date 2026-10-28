#pragma once
#include "document_model/range.h"

namespace ruyijs::document_model {
// A call-scoped preparation of removals against the original live Range list.
// No new registry: Commit changes only each existing Range's boundary points.
class RangeRemovalPlan final {
 public:
  // A null child removes every child; otherwise remove that one child.
  bool Add(State&, NodeKey parent, NodeKey child = {});
  void Commit() noexcept;
 private:
  struct Change { Range* range; BoundaryPoint start, end; };
  std::vector<Change> changes_;
};
}
