// Range boundary geometry over the unique Document Model tree. Positions are
// compared by walking the aggregate's parent/sibling links; no order is cached.
#include "document_model/range.h"

#include "document_model/state.h"

namespace ruyijs::document_model {
namespace {
bool Snapshot(State& state, NodeKey node, NodeSnapshot* value) {
  return state.Check(state.model.Inspect(node, value));
}
// Index of the child of `ancestor` that lies on the path to `node`, which must
// be a proper descendant of ancestor.
bool ChildIndex(State& state, NodeKey ancestor, NodeKey node, uint32_t* result) {
  NodeKey current = node;
  while (!Same(current, ancestor)) {
    NodeSnapshot value;
    if (!Snapshot(state, current, &value)) return false;
    if (Same(value.parent, ancestor)) return RangeIndex(state, ancestor, current, result);
    if (Same(value.parent, {}))
      return TypeError(state.cx, "Nodes do not share a tree");
    current = value.parent;
  }
  *result = 0;
  return true;
}
}

bool RangeIndex(State& state, NodeKey parent, NodeKey child, uint32_t* result) {
  std::vector<NodeKey> children;
  if (!state.Check(state.model.Children(parent, &children))) return false;
  for (size_t index = 0; index < children.size(); ++index)
    if (Same(children[index], child)) { *result = uint32_t(index); return true; }
  return TypeError(state.cx, "Node is not a child of the container");
}

bool RangeIsAncestor(State& state, NodeKey ancestor, NodeKey node, bool* result) {
  NodeKey current = node;
  while (!Same(current, {})) {
    if (Same(current, ancestor)) { *result = true; return true; }
    NodeSnapshot value;
    if (!Snapshot(state, current, &value)) return false;
    if (Same(value.parent, {})) break;
    current = value.parent;
  }
  *result = false;
  return true;
}

bool RangeComparePoints(State& state, const BoundaryPoint& a, const BoundaryPoint& b,
                        int* result) {
  if (Same(a.node, b.node)) {
    *result = a.offset < b.offset ? -1 : a.offset > b.offset ? 1 : 0;
    return true;
  }
  bool ancestor = false;
  uint32_t index = 0;
  if (!RangeIsAncestor(state, a.node, b.node, &ancestor)) return false;
  if (ancestor) {
    if (!ChildIndex(state, a.node, b.node, &index)) return false;
    *result = a.offset <= index ? -1 : 1;
    return true;
  }
  if (!RangeIsAncestor(state, b.node, a.node, &ancestor)) return false;
  if (ancestor) {
    if (!ChildIndex(state, b.node, a.node, &index)) return false;
    *result = index < b.offset ? -1 : 1;
    return true;
  }
  NodeKey common;
  if (!RangeCommonAncestor(state, a, b, &common)) return false;
  uint32_t first = 0, second = 0;
  if (!ChildIndex(state, common, a.node, &first) ||
      !ChildIndex(state, common, b.node, &second)) return false;
  *result = first < second ? -1 : first > second ? 1 : 0;
  return true;
}

bool RangeCommonAncestor(State& state, const BoundaryPoint& a, const BoundaryPoint& b,
                         NodeKey* result) {
  NodeKey node = a.node;
  while (true) {
    bool covers = false;
    if (!RangeIsAncestor(state, node, b.node, &covers)) return false;
    if (covers) { *result = node; return true; }
    NodeSnapshot value;
    if (!Snapshot(state, node, &value)) return false;
    if (Same(value.parent, {})) { *result = {}; return true; }
    node = value.parent;
  }
}

bool RangeContainsNode(State& state, const BoundaryPoint& start, const BoundaryPoint& end,
                       NodeKey node, bool* result) {
  NodeSnapshot value;
  if (!Snapshot(state, node, &value)) return false;
  uint32_t length = value.kind == NodeKind::Text || value.kind == NodeKind::Comment || value.kind == NodeKind::ProcessingInstruction
                        ? uint32_t(value.data_length) : uint32_t(value.child_count);
  int first = 0, second = 0;
  BoundaryPoint low{node, 0}, high{node, length};
  if (!RangeComparePoints(state, low, start, &first)) return false;
  if (!RangeComparePoints(state, high, end, &second)) return false;
  *result = first > 0 && second < 0;
  return true;
}

bool RangeTopLevelContained(State& state, const BoundaryPoint& start,
                            const BoundaryPoint& end, NodeKey root,
                            std::vector<NodeKey>* result) {
  std::vector<NodeKey> stack, children;
  if (!state.Check(state.model.Children(root, &children))) return false;
  for (auto it = children.rbegin(); it != children.rend(); ++it) stack.push_back(*it);
  while (!stack.empty()) {
    NodeKey node = stack.back();
    stack.pop_back();
    bool contained = false;
    if (!RangeContainsNode(state, start, end, node, &contained)) return false;
    if (contained) { result->push_back(node); continue; }
    std::vector<NodeKey> kids;
    if (!state.Check(state.model.Children(node, &kids))) return false;
    for (auto it = kids.rbegin(); it != kids.rend(); ++it) stack.push_back(*it);
  }
  return true;
}
}  // namespace ruyijs::document_model
