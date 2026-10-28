// Cross-Model moves use the original factories, caches and publication boundary.
#include "document_model/foreign_adoption.h"
#include "js/Realm.h"
#include <algorithm>

namespace ruyijs::document_model {
ForeignAdoption::~ForeignAdoption() {
  if (!committed_)
    for (const Draft& draft : draft_.drafts) target_.model.Rollback(draft.token);
  else if (fragmentInsertion_) target_.model.Release(draft_.root);
}

bool ForeignAdoption::Prepare(Wrapper& source, NodeKey document, bool fragment_insertion) {
  source_ = source.owner;
  original_ = source.key;
  fragmentInsertion_ = fragment_insertion;
  if (!target_.Check(target_.model.PrepareAdoption(document, source_->model, original_, &draft_, fragmentInsertion_)))
    return false;
  if (fragmentInsertion_)
    draft_.identities.erase(std::remove_if(draft_.identities.begin(), draft_.identities.end(),
        [&](const auto& pair) { return Same(pair.first, original_); }), draft_.identities.end());
  // A subsequent notification may allocate. Preserve every surviving original
  // wrapper until all of the original cache links have changed ownership.
  for (const auto& pair : draft_.identities) {
    Wrapper* entry = source_->Find(pair.first);
    if (entry && !roots_.append(entry->object)) {
      JS_ReportOutOfMemory(target_.cx);
      return false;
    }
  }
  return true;
}

bool ForeignAdoption::Commit() {
  NodeSnapshot node;
  Status status = source_->model.Inspect(original_, &node);
  if (status.code != ErrorCode::None) return source_->Check(status);
  if (source_ == &target_) {
    if (!target_.Check(target_.model.DetachTransferred(original_, draft_.identities, fragmentInsertion_)))
      return false;
  } else if (fragmentInsertion_) {
    if (!source_->Check(source_->model.ClearFragment(original_))) return false;
  } else if (node.parent.node) {
    // This source removal captures original objects before its native records
    // are retired. The target log is still behind its publication boundary.
    status = source_->model.Remove(node.parent, original_);
    if (status.code != ErrorCode::None) return source_->Check(status);
  }
  for (const Draft& draft : draft_.drafts) target_.model.Publish(draft.token);
  if (source_ == &target_) target_.Rekey(draft_.identities);
  else target_.TakeAdoptedIdentities(*source_, draft_.identities);
  committed_ = true;
  if (source_ != &target_)
    source_->NotifyNodeLifecycle({NodeLifecycleKind::Transferred, draft_.identities, {}, target_.sourceRealm});
  for (const auto& pair : draft_.identities) source_->model.Release(pair.first);
  for (const Draft& draft : draft_.drafts) target_.SyncMembership(draft.root);
  return true;
}

bool AdoptForeign(Wrapper& document, Wrapper& source, JS::MutableHandleValue result) {
  State& target = *document.owner;
  ForeignAdoption transfer(target);
  if (!transfer.Prepare(source, document.key)) return false;
  {
    AdoptionPublication publishing(target);
    if (!transfer.Commit()) return false;
  }
  target.FinishAdoptionPublication();
  if (!target.Check({})) return false;
  return target.Wrap(transfer.root(), result);
}

bool InsertForeign(Wrapper& parent, Wrapper& source, NodeKey before, NodeKey replaced) {
  State& target = *parent.owner;
  NodeSnapshot destination, original;
  if (!source.owner->Check(source.owner->model.Inspect(source.key, &original))) return false;
  if (original.kind == NodeKind::Document)
    return ThrowDOM(target, "HierarchyRequestError", "A Document is not an allowed child");
  if (!target.Check(target.model.Inspect(parent.key, &destination))) return false;
  ForeignAdoption transfer(target);
  const NodeKey document = destination.kind == NodeKind::Document ? parent.key : destination.owner_document;
  if (!transfer.Prepare(source, document, original.kind == NodeKind::DocumentFragment)) return false;
  AdoptionPlacement placement;
  if (!target.Check(target.model.PrepareAdoptionPlacement(parent.key, transfer.root(), before,
                                                          replaced, &placement))) return false;
  {
    AdoptionPublication publishing(target);
    if (!transfer.Commit()) return false;
    const Status status = target.model.CommitAdoptionPlacement(placement);
    if (status.code != ErrorCode::None) return target.Check(status);
  }
  target.FinishAdoptionPublication();
  target.SyncMembership(transfer.root());
  return target.Check({});
}
}  // namespace ruyijs::document_model
