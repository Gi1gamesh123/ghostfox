#include "document_model/traversal.h"

#include "document_model/state.h"

namespace ruyijs::document_model {
namespace {
constexpr int32_t kAccept = 1, kReject = 2;

bool HasChild(State& state, NodeKey node, bool* result) {
  NodeKey child;
  if (!WalkerChild(state, node, true, &child)) return false;
  *result = WalkerPresent(child);
  return true;
}
}  // namespace

// parentNode(): the first accepted ancestor, the walker's root included.
bool WalkerAncestor(State& state, Walker& walker, JS::MutableHandleValue result) {
  NodeKey node = walker.reference;
  while (!Same(node, walker.root)) {
    NodeKey parent;
    if (!WalkerParent(state, node, &parent) || !WalkerPresent(parent)) break;
    node = parent;
    int32_t value = 0;
    if (!FilterNode(state, walker, node, &value)) return false;
    if (value == kAccept) {
      walker.reference = node;
      return state.Wrap(node, result);
    }
  }
  result.setNull();
  return true;
}

// firstChild()/lastChild(): descend while the filter does not reject.
bool WalkerDescend(State& state, Walker& walker, bool first, JS::MutableHandleValue result) {
  NodeKey node = walker.reference;
  int32_t value = kAccept;
  while (value != kReject) {
    bool has = false;
    if (!HasChild(state, node, &has) || !has) break;
    NodeKey child;
    if (!WalkerChild(state, node, first, &child)) return false;
    node = child;
    if (!FilterNode(state, walker, node, &value)) return false;
    if (value == kAccept) {
      walker.reference = node;
      return state.Wrap(node, result);
    }
  }
  result.setNull();
  return true;
}

// previousSibling()/nextSibling(): the next accepted sibling only, no ascent.
bool WalkerAdjacent(State& state, Walker& walker, bool next, JS::MutableHandleValue result) {
  NodeKey node = walker.reference;
  if (Same(node, walker.root)) {
    result.setNull();
    return true;
  }
  NodeKey sibling;
  if (!WalkerSibling(state, node, next, &sibling)) return false;
  while (WalkerPresent(sibling)) {
    node = sibling;
    int32_t value = 0;
    if (!FilterNode(state, walker, node, &value)) return false;
    if (value == kAccept) {
      walker.reference = node;
      return state.Wrap(node, result);
    }
    if (!WalkerSibling(state, node, next, &sibling)) return false;
  }
  result.setNull();
  return true;
}

// previousNode(): the deepest accepted node before the current one.
bool WalkerBackward(State& state, Walker& walker, JS::MutableHandleValue result) {
  NodeKey node = walker.reference;
  while (!Same(node, walker.root)) {
    NodeKey sibling;
    if (!WalkerSibling(state, node, false, &sibling)) return false;
    while (WalkerPresent(sibling)) {
      node = sibling;
      int32_t value = 0;
      if (!FilterNode(state, walker, node, &value)) return false;
      bool has = false;
      while (value != kReject) {
        if (!HasChild(state, node, &has) || !has) break;
        NodeKey child;
        if (!WalkerChild(state, node, false, &child)) return false;
        node = child;
        if (!FilterNode(state, walker, node, &value)) return false;
      }
      if (value == kAccept) {
        walker.reference = node;
        return state.Wrap(node, result);
      }
      if (!WalkerSibling(state, node, false, &sibling)) return false;
    }
    NodeKey parent;
    if (!WalkerParent(state, node, &parent) || !WalkerPresent(parent)) break;
    node = parent;
    int32_t value = 0;
    if (!FilterNode(state, walker, node, &value)) return false;
    if (value == kAccept) {
      walker.reference = node;
      return state.Wrap(node, result);
    }
  }
  result.setNull();
  return true;
}

// nextNode(): the next accepted node, never the root itself.
bool WalkerForward(State& state, Walker& walker, JS::MutableHandleValue result) {
  NodeKey node = walker.reference;
  int32_t value = kAccept;
  while (true) {
    bool has = false;
    while (value != kReject) {
      if (!HasChild(state, node, &has) || !has) break;
      NodeKey child;
      if (!WalkerChild(state, node, true, &child)) return false;
      node = child;
      if (!FilterNode(state, walker, node, &value)) return false;
      if (value == kAccept) {
        walker.reference = node;
        return state.Wrap(node, result);
      }
    }
    NodeKey sibling, temporary = node;
    bool advanced = false;
    while (WalkerPresent(temporary)) {
      if (Same(temporary, walker.root)) {
        result.setNull();
        return true;
      }
      if (!WalkerSibling(state, temporary, true, &sibling)) return false;
      if (WalkerPresent(sibling)) {
        node = sibling;
        advanced = true;
        break;
      }
      NodeKey parent;
      if (!WalkerParent(state, temporary, &parent)) return false;
      temporary = parent;
    }
    if (!advanced) {
      result.setNull();
      return true;
    }
    // The sibling walk landed outside the previous subtree; accept it now,
    // otherwise the loop descends into it on a skip.
    if (!FilterNode(state, walker, node, &value)) return false;
    if (value == kAccept) {
      walker.reference = node;
      return state.Wrap(node, result);
    }
  }
}
}  // namespace ruyijs::document_model
