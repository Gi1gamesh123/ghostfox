#include "document_model/range_removal.h"
#include "document_model/state.h"
#include <algorithm>

namespace ruyijs::document_model {
namespace {
bool Adjust(State& state, NodeKey parent, NodeKey child, uint32_t index, BoundaryPoint* point) {
  if (point->node.document != parent.document) return true;
  if (!child.node && Same(point->node, parent)) { point->offset = 0; return true; }
  bool inside = false;
  const auto status = state.model.AncestorOrSelf(child.node ? child : parent, point->node, &inside);
  if (status.code != ErrorCode::None) return state.Check(status);
  if (inside) *point = {parent, child.node ? index : 0};
  else if (child.node && Same(point->node, parent) && point->offset > index) --point->offset;
  return true;
}
}

bool RangeRemovalPlan::Add(State& state, NodeKey parent, NodeKey child) {
  if (!parent.node || (!state.ranges && state.guests.empty())) return true;
  uint32_t index = 0;
  if (child.node && !RangeIndex(state, parent, child, &index)) return false;
  // Own ranges still in this tree, then other realms' ranges moved into it.
  std::vector<Range*> live;
  for (Range* range = state.ranges.get(); range; range = range->next.get())
    if (!range->tree) live.push_back(range);
  live.insert(live.end(), state.guests.begin(), state.guests.end());
  for (Range* range : live) {
    auto found = std::find_if(changes_.begin(), changes_.end(),
        [&](const Change& change) { return change.range == range; });
    BoundaryPoint start = found == changes_.end() ? range->start : found->start;
    BoundaryPoint end = found == changes_.end() ? range->end : found->end;
    if (!Adjust(state, parent, child, index, &start) || !Adjust(state, parent, child, index, &end))
      return false;
    if (found == changes_.end()) changes_.push_back({range, start, end});
    else { found->start = start; found->end = end; }
  }
  return true;
}

void RangeRemovalPlan::Commit() noexcept {
  for (const auto& change : changes_) {
    change.range->start = change.start;
    change.range->end = change.end;
  }
}
}
