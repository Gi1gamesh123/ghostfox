// TreeWalker/NodeIterator shared machinery. Domain owner: the existing
// document_model tree; a walker keeps no node list or cached order, it
// re-derives every step from the unique DocumentAggregate.
#ifndef RUYIJS_DOCUMENT_TRAVERSAL_H
#define RUYIJS_DOCUMENT_TRAVERSAL_H
#include <cstdint>
#include <memory>

#include "document_model/model.h"
#include "js/CallArgs.h"
#include "js/Class.h"
#include "js/RootingAPI.h"

namespace ruyijs::document_model {
struct State;

// One native class behind both traversal objects. The reserved slot holds the
// realm-owned Walker below.
extern const JSClass kWalkerClass;

// TreeWalker and NodeIterator differ only in navigation state; both are
// realm-owned like Collection, so the rooted object/filter die with the realm
// instead of in a JS finalizer. `filter` is the script-supplied NodeFilter
// (the getter's value), `callback` the callable it resolved to.
struct Walker {
  JS::PersistentRootedObject object, filter, callback;
  // The realm that created this walker; a foreign realm's member table must
  // not operate on it, so the brand check keeps the realm boundary.
  State* owner = nullptr;
  bool iterator = false;
  NodeKey root, reference;
  uint32_t what = 0xFFFFFFFF;
  bool before = true;  // NodeIterator: the pointer sits before `reference`.
  std::unique_ptr<Walker> next;
  Walker(JSContext* cx, bool is_iterator)
      : object(cx), filter(cx), callback(cx), iterator(is_iterator) {}
};

// The walker behind a receiver, or a TypeError for a foreign receiver.
Walker* WalkerOf(State&, const JS::CallArgs&);
// Applies whatToShow then the user filter; 1 accept, 2 reject, 3 skip.
bool FilterNode(State&, Walker&, NodeKey, int32_t* result);

// Single-node reads shared by both member tables; each one inspects the unique
// tree, so a walker never keeps a child or sibling index.
bool WalkerPresent(NodeKey);
bool WalkerChild(State&, NodeKey, bool first, NodeKey* result);
bool WalkerSibling(State&, NodeKey, bool next, NodeKey* result);
bool WalkerParent(State&, NodeKey, NodeKey* result);

// TreeWalker steps (walker_steps.cpp), each returning a node or null.
bool WalkerAncestor(State&, Walker&, JS::MutableHandleValue);
bool WalkerDescend(State&, Walker&, bool first, JS::MutableHandleValue);
bool WalkerAdjacent(State&, Walker&, bool next, JS::MutableHandleValue);
bool WalkerForward(State&, Walker&, JS::MutableHandleValue);
bool WalkerBackward(State&, Walker&, JS::MutableHandleValue);

// document.createTreeWalker(root, whatToShow, filter). Root must be a Node of
// this realm; whatToShow defaults to SHOW_ALL and filter may be null, a
// callable, or an object carrying a callable acceptNode.
bool CreateTreeWalker(State&, JS::HandleValue root, JS::HandleValue what,
                      JS::HandleValue filter, JS::MutableHandleValue result);
bool CreateNodeIterator(State&, JS::HandleValue root, JS::HandleValue what,
                        JS::HandleValue filter, JS::MutableHandleValue result);
bool InstallNodeFilter(State&);
bool InstallTreeWalker(State&);
bool InstallNodeIterator(State&);
}  // namespace ruyijs::document_model
#endif
