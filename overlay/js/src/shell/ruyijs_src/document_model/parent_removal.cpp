#include "document_model/parent_removal.h"
#include <algorithm>

namespace ruyijs::document_model {
bool ParentRemovalPlan::Add(State& state, NodeKey parent, NodeKey child) {
  NodeSnapshot owner;
  if (!state.Check(state.model.Inspect(parent, &owner))) return false;
  Removal value{&state, owner.kind == NodeKind::Document ? parent : owner.owner_document, {}, {}};
  std::vector<NodeKey> roots;
  if (child.node) roots.push_back(child);
  else if (!state.Check(state.model.Children(parent, &roots))) return false;
  for (NodeKey root : roots) {
    std::vector<NodeKey> subtree;
    if (!state.Check(state.model.Subtree(root, &subtree))) return false;
    value.nodes.insert(value.nodes.end(), subtree.begin(), subtree.end());
  }
  NodeKey focused;
  if (!state.Check(state.model.FocusedElement(value.document, &focused))) return false;
  if (std::any_of(value.nodes.begin(), value.nodes.end(), [&](NodeKey key) { return Same(key, focused); }))
    value.focused = focused;
  removals_.push_back(std::move(value));
  return true;
}

bool ParentRemovalPlan::Commit() {
  for (auto& removal : removals_) {
    removal.state->DiscardFramesIn(removal.nodes);
    if (removal.focused.node && !removal.state->Check(
        removal.state->model.SetFocusedElement(removal.document, {}))) return false;
  }
  return true;
}
}
