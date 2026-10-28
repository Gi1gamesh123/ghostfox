// Range content algorithms: delete/extract/clone/insert and toString. Each step
// re-derives order and containment from the unique tree; a DocumentFragment is
// the one second tree the specification itself produces.
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
bool CharacterData(const NodeSnapshot& value) {
  return value.kind == NodeKind::Text || value.kind == NodeKind::Comment || value.kind == NodeKind::ProcessingInstruction;
}
bool Collapsed(const Range& range) {
  return Same(range.start.node, range.end.node) && range.start.offset == range.end.offset;
}
// Creates a detached Text node and moves it to the end of the fragment. A null
// fragment (deleteContents) discards it: only the truncation is kept.
bool AppendText(State& state, NodeKey document, NodeKey fragment, const DomString& data) {
  if (data.empty() || Same(fragment, {})) return true;
  Draft draft;
  CreateSpec spec{NodeKind::Text, {}, data};
  if (!state.Check(state.model.Create(document, spec, &draft))) return false;
  if (!state.Check(state.model.Insert(fragment, draft.root, {}))) {
    state.model.Rollback(draft.token);
    return false;
  }
  state.model.Publish(draft.token);
  return true;
}
// Clones one subtree through the same clone path as importNode, then re-parents
// the detached clone into the fragment.
bool CloneInto(State& state, NodeKey fragment, NodeKey node) {
  Draft draft;
  if (!state.Check(state.model.Clone(node, true, &draft))) return false;
  if (!state.Check(state.model.Insert(fragment, draft.root, {}))) {
    state.model.Rollback(draft.token);
    return false;
  }
  state.model.Publish(draft.token);
  return true;
}
// Splits a Text node at range.start, returning the new tail node ({0,0} when
// the whole node is already before the offset).
bool SplitText(State& state, Range& range, NodeKey* result) {
  NodeSnapshot value;
  if (!Snapshot(state, range.start.node, &value)) return false;
  DomString data;
  if (!Data(state, range.start.node, value, &data)) return false;
  *result = {};
  if (range.start.offset >= data.size()) return true;
  Draft draft;
  CreateSpec spec{NodeKind::Text, {}, {}};
  if (!state.Check(state.model.Create(range.document, spec, &draft))) return false;
  JS::RootedValue rooted(state.cx);
  if (!state.Commit(draft, &rooted) ||
      !state.Check(state.model.SplitTextInto(range.start.node, draft.root, range.start.offset))) return false;
  *result = draft.root;
  return true;
}
// Shared body of extractContents (remove=true, fragment) and deleteContents
// (remove=true, null fragment) and cloneContents (remove=false).
bool SelectInto(State& state, Range& range, bool remove, NodeKey fragment) {
  if (Collapsed(range)) return true;
  NodeSnapshot start, end;
  if (!Snapshot(state, range.start.node, &start)) return false;
  if (!Snapshot(state, range.end.node, &end)) return false;
  if (Same(range.start.node, range.end.node) && CharacterData(start)) {
    DomString data;
    if (!Data(state, range.start.node, start, &data)) return false;
    DomString selected = data.substr(range.start.offset, range.end.offset - range.start.offset);
    if (remove && !state.Check(state.model.ReplaceData(range.start.node, range.start.offset,
                                                       range.end.offset - range.start.offset, u"")))
      return false;
    return AppendText(state, range.document, fragment, selected);
  }
  if (CharacterData(start)) {
    DomString data;
    if (!Data(state, range.start.node, start, &data)) return false;
    DomString tail = data.substr(range.start.offset);
    if (remove && !state.Check(state.model.ReplaceData(range.start.node, range.start.offset,
                                                       uint32_t(data.size()) - range.start.offset, u"")))
      return false;
    if (!AppendText(state, range.document, fragment, tail)) return false;
  }
  std::vector<NodeKey> contained;
  if (!RangeTopLevelContained(state, range.start, range.end, start.root, &contained))
    return false;
  for (NodeKey node : contained) {
    if (!remove) { if (!CloneInto(state, fragment, node)) return false; continue; }
    if (Same(fragment, {})) {
      NodeSnapshot value;
      if (!Snapshot(state, node, &value)) return false;
      if (!state.Check(state.model.Remove(value.parent, node))) return false;
    } else if (!state.Check(state.model.Insert(fragment, node, {}))) return false;
  }
  if (CharacterData(end)) {
    DomString data;
    if (!Data(state, range.end.node, end, &data)) return false;
    DomString head = data.substr(0, range.end.offset);
    if (remove && !state.Check(state.model.ReplaceData(range.end.node, 0, range.end.offset, u"")))
      return false;
    if (!AppendText(state, range.document, fragment, head)) return false;
  }
  return true;
}
}

bool RangeDeleteContents(State& state, Range& range) {
  BoundaryPoint start = range.start;
  if (!SelectInto(state, range, true, {})) return false;
  range.start = range.end = start;
  return true;
}

bool RangeExtractContents(State& state, Range& range, JS::MutableHandleValue result) {
  BoundaryPoint start = range.start;
  Draft fragment;
  if (!state.Check(state.model.CreateFragment(range.document, &fragment))) return false;
  if (!SelectInto(state, range, true, fragment.root)) {
    state.model.Rollback(fragment.token);
    return false;
  }
  range.start = range.end = start;
  return state.Commit(fragment, result);
}

bool RangeCloneContents(State& state, Range& range, JS::MutableHandleValue result) {
  Draft fragment;
  if (!state.Check(state.model.CreateFragment(range.document, &fragment))) return false;
  if (!SelectInto(state, range, false, fragment.root)) {
    state.model.Rollback(fragment.token);
    return false;
  }
  return state.Commit(fragment, result);
}

bool RangeInsertNode(State& state, Range& range, JS::HandleValue nodeValue) {
  NodeKey node;
  if (!NodeArgument(state, nodeValue, &node)) return false;
  NodeSnapshot start;
  if (!Snapshot(state, range.start.node, &start)) return false;
  if (start.kind == NodeKind::Comment || start.kind == NodeKind::ProcessingInstruction)
    return ThrowDOM(state, "HierarchyRequestError", "Range start is a comment");
  if (start.kind == NodeKind::Text && Same(start.parent, {}))
    return ThrowDOM(state, "HierarchyRequestError", "Range start text has no parent");
  bool ancestor = false;
  if (!RangeIsAncestor(state, node, range.start.node, &ancestor)) return false;
  if (ancestor)
    return ThrowDOM(state, "HierarchyRequestError", "Node is an ancestor of the range");
  NodeKey reference, parent = start.kind == NodeKind::Text ? start.parent : range.start.node;
  if (start.kind == NodeKind::Text) {
    reference = range.start.node;
  } else {
    std::vector<NodeKey> children;
    if (!state.Check(state.model.Children(range.start.node, &children))) return false;
    if (range.start.offset < children.size()) reference = children[range.start.offset];
  }
  if (start.kind == NodeKind::Text && !SplitText(state, range, &reference)) return false;
  if (Same(node, reference)) {
    NodeSnapshot value;
    if (!Snapshot(state, reference, &value)) return false;
    reference = value.next_sibling;
  }
  NodeSnapshot value;
  if (!Snapshot(state, node, &value)) return false;
  if (!Same(value.parent, {}) && !state.Check(state.model.Remove(value.parent, node))) return false;
  uint32_t offset = 0;
  if (Same(reference, {})) {
    NodeSnapshot container;
    if (!Snapshot(state, parent, &container)) return false;
    offset = uint32_t(container.child_count);
  } else if (!RangeIndex(state, parent, reference, &offset)) return false;
  if (!state.Check(state.model.Insert(parent, node, reference))) return false;
  if (Same(range.start.node, range.end.node) && range.start.offset == range.end.offset)
    range.end = BoundaryPoint{parent, offset + 1};
  range.start = BoundaryPoint{parent, offset};
  return true;
}
}  // namespace ruyijs::document_model
