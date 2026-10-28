// Range.toString (A6.5). Reads the unique tree in order and concatenates the
// data of every Text node the boundaries enclose, whole or partial.
#include "document_model/range.h"

#include "document_model/state.h"

namespace ruyijs::document_model {
namespace {
bool Snapshot(State& state, NodeKey node, NodeSnapshot* value) {
  return state.Check(state.model.Inspect(node, value));
}
bool Data(State& state, NodeKey node, const NodeSnapshot& value, DomString* result) {
  return state.Check(state.model.SubstringData(node, 0, uint32_t(value.data_length), result));
}
bool CollectText(State& state, const BoundaryPoint& start, const BoundaryPoint& end,
                 NodeKey node, DomString* out) {
  NodeSnapshot value;
  if (!Snapshot(state, node, &value)) return false;
  if (value.kind == NodeKind::Text) {
    DomString data;
    if (!Data(state, node, value, &data)) return false;
    if (Same(node, start.node) && Same(node, end.node))
      *out += data.substr(start.offset, end.offset - start.offset);
    else if (Same(node, start.node)) *out += data.substr(start.offset);
    else if (Same(node, end.node)) *out += data.substr(0, end.offset);
    else {
      bool contained = false;
      if (!RangeContainsNode(state, start, end, node, &contained)) return false;
      if (contained) *out += data;
    }
    return true;
  }
  if (value.kind != NodeKind::Element && value.kind != NodeKind::Document &&
      value.kind != NodeKind::DocumentFragment) return true;
  std::vector<NodeKey> children;
  if (!state.Check(state.model.Children(node, &children))) return false;
  for (NodeKey child : children)
    if (!CollectText(state, start, end, child, out)) return false;
  return true;
}
}

bool RangeText(State& state, Range& range, JS::MutableHandleValue result) {
  NodeSnapshot value;
  if (!Snapshot(state, range.start.node, &value)) return false;
  DomString text;
  if (!CollectText(state, range.start, range.end, value.root, &text)) return false;
  return Value(state.cx, DomStringView(text), result);
}
}  // namespace ruyijs::document_model
