// Node-wrapper reclamation (T41). Owner: document_model.
//
// A wrapper is one JS object for one immutable NodeKey. A node the unique tree
// holds (is_connected) owns its wrapper: the wrapper is rooted, so identity and
// listeners survive every collection, exactly as before. A node the tree no
// longer holds keeps its wrapper only while script references it: the wrapper
// is not rooted, and when the engine collects it the class finalizer marks the
// record and raises reclaimPending. The next State::Wrap - a point where the
// engine is quiescent and this owner may allocate and call JSAPI - then drops
// the borrowed Interaction record and releases the domain record, so the 100000
// node and 100000 target limits only ever bound nodes that are still reachable.
//
// This file adds no tree, no identity source and no reachability index: the
// wrapper list is the realm's only identity source, and Model reads are the
// only tree reads.
#include "document_model/state.h"
#include "runtime/weak_object.h"
#include "document_model/style_source_values.h"
#include "interaction/realm.h"
#include "js/GCAPI.h"
#include <set>
#include <utility>
#include <vector>

namespace ruyijs::document_model {
void NotifyTree(void* context, DocumentId, const std::vector<NodeKey>& added,
                const std::vector<NodeKey>& removed) {
  State* state = static_cast<State*>(context);
  for (NodeKey key : added) state->SyncMembership(key);
  for (NodeKey key : removed) state->SyncMembership(key);
  CustomElementsTreeChanged(*state, added, removed);
}
// The node cap is about live nodes, so a cap check that would fail first runs
// one real collection: the finalizers mark every wrapper the engine dropped and
// the reclaim below frees their domain and Interaction records. budget() only
// calls this before applying a mutation, so no half-applied tree is visible.
void ReclaimNow(void* context) {
  State& state = *static_cast<State*>(context);
  JS::PrepareForFullGC(state.cx);
  JS::NonIncrementalGC(state.cx, JS::GCOptions::Shrink, JS::GCReason::API);
  state.ReclaimPending();
}
// One subtree walk per changed scope. Connectivity is read back from the tree
// instead of being inferred from the change's direction: a subtree inserted
// into a detached parent is still detached, and an adopt that reorders the
// events must not root a node the document does not hold. Every node below
// `scope` shares `scope`'s root, so one read settles the whole subtree.
void State::SyncMembership(NodeKey scope, bool retireStyle) {
  const bool unpublished=model.IdentityTransferPending();
  if (unpublished) {
    bool seen=false;
    for (NodeKey pending:deferredStyleScopes) if (Same(pending,scope)) { seen=true;break; }
    if (!seen) deferredStyleScopes.push_back(scope);
  }
  NodeKey root;
  bool connected = false;
  if (model.Root(scope, &root, &connected).code != ErrorCode::None) return;
  std::vector<NodeKey> keys;
  if (model.Subtree(scope, &keys).code != ErrorCode::None) keys.assign(1, scope);
  for (NodeKey key : keys) {
    if (connected) detached.erase(key); else detached[key] = true;
    auto found = wrapperIndex.find(key);
    if (found != wrapperIndex.end() && !found->second->dead && WeakLive(found->second->object)) {
      found->second->Root(cx, connected);
      JS::RootedObject object(cx, found->second->object);
      if (!RefreshShadowScope(*this, key, object)) return;
      if (retireStyle && !NotifyStyleAssociations(*this, *found->second)) return;
    }
    if (!unpublished && !NotifyLinkStyle(*this, key)) return;
  }
}
// A live wrapper anywhere in the subtree keeps the whole subtree: the node with
// the wrapper is reachable from script, and so is the path of parents that node
// objects expose through parentNode.
bool State::Pinned(NodeKey scope) const {
  std::vector<NodeKey> keys;
  if (model.Subtree(scope, &keys).code != ErrorCode::None) return false;
  for (NodeKey key : keys) {
    auto found = wrapperIndex.find(key);
    if (found != wrapperIndex.end() && !found->second->dead) return true;
  }
  return false;
}
// `root` is the topmost node of one detached tree. Everything script can reach
// from one live wrapper - that wrapper's node, its children, its parents and
// their other subtrees - lies inside this one tree, so the tree is kept or
// released as a whole. A connected root belongs to the document and is never
// released, and a released identity is already gone.
void State::ReleaseDetached(NodeKey root, bool connected) {
  if (connected) return;
  const bool pinned = Pinned(root);
  if (pinned) return;
  if (!nodeLifecycleConsumer) { model.Release(root); return; }
  std::vector<NodeKey> keys;
  if (model.Subtree(root, &keys).code != ErrorCode::None || keys.empty()) return;
  if (model.Release(root).code == ErrorCode::None)
    NotifyNodeLifecycle({NodeLifecycleKind::Released, {}, keys});
}
void State::ReclaimPending() {
  reclaimPending = false;
  // Collection finalizers only clear weak object addresses. Reap their native
  // records here before a dead scope can be released or any view refreshed.
  auto* collectionSlot = &collections;
  while (*collectionSlot) {
    if ((*collectionSlot)->object) collectionSlot = &(*collectionSlot)->next;
    else {
      auto expired = std::move(*collectionSlot);
      *collectionSlot = std::move(expired->next);
    }
  }
  std::vector<NodeKey> dead;
  // One pass over the wrapper list. Dropping a collected record frees it, so
  // the walk keeps the link to the successor in the unique_ptr slot and never
  // reads the collected record again (T41).
  std::unique_ptr<Wrapper>* slot = &wrappers;
  while (*slot) {
    Wrapper* entry = slot->get();
    if (!entry->dead) {
      slot = &entry->next;
      continue;
    }
    // The finalizer already marked the borrowed Interaction record released -
    // that is what made this wrapper dead - so no second scan of the target
    // registry is needed here.
    dead.push_back(entry->key);
    auto indexed = wrapperIndex.find(entry->key);
    if (indexed != wrapperIndex.end() && indexed->second == entry)
      wrapperIndex.erase(indexed);
    *slot = std::move(entry->next);
  }
  // One release attempt per detached tree: resolving the root first is what
  // keeps a collected child of a live parent - a node script still reads
  // through firstChild - in the same decision as its parent.
  std::set<NodeKey, NodeKeyLess> tried;
  std::set<NodeKey, NodeKeyLess> templateKept;
  std::vector<NodeKey> templateCandidates;
  if (!DocumentReachability(*this, &templateKept, &templateCandidates)) return;
  const auto release = [&](NodeKey key) {
    NodeKey root;
    bool connected = false;
    if (model.Root(key, &root, &connected).code != ErrorCode::None) return;
    if (!tried.insert(root).second) return;
    if (templateKept.count(root)) return;
    ReleaseDetached(root, connected);
  };
  for (NodeKey key : dead) release(key);
  for (NodeKey key : templateCandidates) release(key);
  // A detached node that never had a wrapper has no finalizer to start from, so
  // every root the change point reported is offered to the same rule. The walk
  // is idempotent: a released, connected or pinned tree simply drops out.
  for (auto entry = detached.begin(); entry != detached.end();) {
    NodeKey key = entry->first;
    entry = detached.erase(entry);
    release(key);
  }
}
}  // namespace ruyijs::document_model
