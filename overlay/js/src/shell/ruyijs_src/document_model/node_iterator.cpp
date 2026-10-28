#include "document_model/traversal.h"

#include "document_model/state.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::document_model {
namespace {
enum Field { Root, Reference, Before, WhatToShow, Filter };
enum Method { NextNode, PreviousNode, Detach };

constexpr int32_t kAccept = 1, kReject = 2;

bool Snapshot(State& state, NodeKey node, NodeSnapshot* result) {
  return state.Check(state.model.Inspect(node, result));
}

// The next node after `node` inside the walker's root, in tree order.
bool NextInOrder(State& state, NodeKey root, NodeKey node, NodeKey* result, bool* moved) {
  NodeSnapshot value;
  if (!Snapshot(state, node, &value)) return false;
  if (WalkerPresent(value.first_child)) {
    *result = value.first_child;
    *moved = true;
    return true;
  }
  while (!Same(node, root)) {
    NodeSnapshot current;
    if (!Snapshot(state, node, &current)) return false;
    if (WalkerPresent(current.next_sibling)) {
      *result = current.next_sibling;
      *moved = true;
      return true;
    }
    if (!WalkerPresent(current.parent)) break;
    node = current.parent;
  }
  *moved = false;
  return true;
}

// The deepest last descendant of `node`; lets a rejected subtree be skipped.
bool DeepestLast(State& state, NodeKey node, NodeKey* result) {
  while (true) {
    NodeSnapshot value;
    if (!Snapshot(state, node, &value)) return false;
    if (!WalkerPresent(value.last_child)) break;
    node = value.last_child;
  }
  *result = node;
  return true;
}

// The node before `node` in tree order, from within the walker's root.
bool PreviousInOrder(State& state, NodeKey root, NodeKey node, NodeKey* result, bool* moved) {
  *moved = false;
  if (Same(node, root)) return true;
  NodeSnapshot value;
  if (!Snapshot(state, node, &value)) return false;
  if (WalkerPresent(value.previous_sibling)) {
    if (!DeepestLast(state, value.previous_sibling, result)) return false;
    *moved = true;
    return true;
  }
  if (!WalkerPresent(value.parent)) return true;
  *result = value.parent;
  *moved = true;
  return true;
}

// nextNode(): moves forward from the reference, or returns the reference
// itself while the pointer still sits before it.
bool Next(State& state, Walker& walker, JS::MutableHandleValue result) {
  NodeKey node = walker.reference;
  bool before = walker.before;
  while (true) {
    if (!before) {
      NodeKey next;
      bool moved = false;
      if (!NextInOrder(state, walker.root, node, &next, &moved)) return false;
      if (!moved) {
        result.setNull();
        return true;
      }
      node = next;
    } else {
      before = false;
    }
    int32_t value = 0;
    if (!FilterNode(state, walker, node, &value)) return false;
    if (value == kAccept) {
      walker.reference = node;
      walker.before = false;
      return state.Wrap(node, result);
    }
    if (value == kReject) {
      NodeKey last;
      if (!DeepestLast(state, node, &last)) return false;
      node = last;
    }
  }
}

// previousNode(): mirrors nextNode, leaving the pointer before the result.
bool Previous(State& state, Walker& walker, JS::MutableHandleValue result) {
  NodeKey node = walker.reference;
  bool before = walker.before;
  while (true) {
    if (before) {
      NodeKey previous;
      bool moved = false;
      if (!PreviousInOrder(state, walker.root, node, &previous, &moved)) return false;
      if (!moved) {
        result.setNull();
        return true;
      }
      node = previous;
    } else {
      before = true;
    }
    int32_t value = 0;
    if (!FilterNode(state, walker, node, &value)) return false;
    if (value == kAccept) {
      walker.reference = node;
      walker.before = true;
      return state.Wrap(node, result);
    }
  }
}

bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  Walker* walker = WalkerOf(state, args);
  if (!walker) return false;
  switch (FunctionSlot(args)) {
    case Root: return state.Wrap(walker->root, args.rval());
    case Reference: return state.Wrap(walker->reference, args.rval());
    case Before: args.rval().setBoolean(walker->before); return true;
    case WhatToShow: args.rval().setNumber(double(walker->what)); return true;
    case Filter: args.rval().setObjectOrNull(walker->filter.get()); return true;
  }
  return false;
}

bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  Walker* walker = WalkerOf(state, args);
  if (!walker) return false;
  if (FunctionSlot(args) == Detach) {
    args.rval().setUndefined();
    return true;
  }
  return FunctionSlot(args) == NextNode ? Next(state, *walker, args.rval())
                                        : Previous(state, *walker, args.rval());
}
}  // namespace

bool InstallNodeIterator(State& state) {
  // Firefox's WebIDL binding defines methods before attributes. Keep this
  // order at the owning installer; the shared assembly links constructor last.
  if (!state.Method("NodeIterator", "nextNode", Call, 0, NextNode) ||
      !state.Method("NodeIterator", "previousNode", Call, 0, PreviousNode) ||
      !state.Method("NodeIterator", "detach", Call, 0, Detach)) return false;
  struct Property { const char* name; Field field; };
  static const Property properties[] = {
    {"root", Root}, {"referenceNode", Reference},
    {"pointerBeforeReferenceNode", Before}, {"whatToShow", WhatToShow}, {"filter", Filter}
  };
  for (const Property& property : properties)
    if (!state.Accessor("NodeIterator", property.name, Get, property.field)) return false;
  return true;
}
}  // namespace ruyijs::document_model
