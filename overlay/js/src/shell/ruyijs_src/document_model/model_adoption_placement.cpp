// Validate through the original domain operation, retaining only its prepared
// child order. No port, script or GC runs during this unpublished validation.
#include "document_model/model_internal.h"

namespace ruyijs::document_model {
namespace {
struct RestorePlacement {
  domain::DocumentAggregate& owner;
  domain::Node& parent;
  domain::Node& root;
  domain::Node* replaced;
  domain::NodeId replacedParent;
  std::vector<domain::NodeId> parentChildren, fragmentChildren;
  bool fragment;
  RestorePlacement(domain::DocumentAggregate& value, domain::Node& p, domain::Node& n,
                    domain::Node* old, bool is_fragment)
      : owner(value), parent(p), root(n), replaced(old), replacedParent(old ? old->parent_id() : 0),
        parentChildren(p.child_ids()),
        fragmentChildren(is_fragment ? n.child_ids() : std::vector<domain::NodeId>{}),
        fragment(is_fragment) {}
  ~RestorePlacement() {
    parent.replace_children(parentChildren);
    root.set_parent(0);
    if (replaced) replaced->set_parent(replacedParent);
    if (fragment) {
      root.replace_children(fragmentChildren);
      for (auto id : root.child_ids()) owner.require(id).set_parent(root.id());
    }
  }
};
}

Status Model::PrepareAdoptionPlacement(NodeKey parent, NodeKey root, NodeKey before,
                                       NodeKey replaced, AdoptionPlacement* result) noexcept {
  return Protect([&] {
    auto& owner = State().owner(parent);
    auto& node = owner.require(root.node);
    if (parent.document != root.document || node.parent_id())
      throw domain::DomError(domain::DomErrorCode::InvalidState, "Adoption draft is not detached");
    if ((!Null(before) && before.document != parent.document) ||
        (!Null(replaced) && replaced.document != parent.document))
      throw domain::DomError(domain::DomErrorCode::NotFound, "Reference belongs to another document");
    AdoptionPlacement prepared;
    prepared.parent = parent;
    prepared.root = root;
    prepared.replaced = replaced;
    prepared.fragment = node.node_type() == domain::NodeType::DocumentFragment;
    prepared.added = prepared.fragment ? node.child_ids() : std::vector<domain::NodeId>{root.node};
    prepared.children.reserve(owner.require(parent.node).child_count() + prepared.added.size());
    {
      RestorePlacement restore(owner, owner.require(parent.node), node,
                                replaced.node ? &owner.require(replaced.node) : nullptr, prepared.fragment);
      if (replaced.node) owner.replace_child(parent.node, root.node, replaced.node);
      else owner.insert_before(parent.node, root.node, before.node);
      prepared.children.assign(owner.require(parent.node).child_ids().begin(),
                                owner.require(parent.node).child_ids().end());
    }
    *result = std::move(prepared);
  });
}

Status Model::CommitAdoptionPlacement(AdoptionPlacement& prepared) noexcept {
  return Protect([&] {
    auto& owner = State().owner(prepared.parent);
    owner.require(prepared.parent.node).replace_children(prepared.children);
    if (prepared.fragment) {
      std::vector<domain::NodeId> empty;
      owner.require(prepared.root.node).replace_children(empty);
    }
    for (auto id : prepared.added) owner.require(id).set_parent(prepared.parent.node);
    if (prepared.replaced.node) owner.require(prepared.replaced.node).set_parent(0);
    State().Touch();
    // Original registrations already follow their nodes at this point.
    if (prepared.fragment && !prepared.added.empty())
      State().NoteRemoved(prepared.root, prepared.added, {}, {});
    if (prepared.replaced.node)
      State().NoteReplace(prepared.parent, prepared.added, {prepared.replaced.node});
    else State().NoteInserted(prepared.parent, prepared.added);
  });
}
}  // namespace ruyijs::document_model
