#include "document_model/model_internal.h"

namespace ruyijs::document_model {
namespace {
bool IsAncestor(const domain::DocumentAggregate& owner, domain::NodeId ancestor, domain::NodeId node) {
  for (domain::NodeId current = node; current; current = owner.require(current).parent_id())
    if (current == ancestor) return true;
  return false;
}
}
Status Model::InsertItems(NodeKey target, InsertMode mode, const std::vector<Item>& items,
                     std::vector<std::pair<NodeKey, NodeKey>>* rekey) noexcept {
  return Protect([&] {
    bool foreign=false;
    for (const Item& item:items) if (!Null(item.node) && item.node.document!=target.document) foreign=true;
    IdentityTransferScope publication(State().identityTransfer,foreign);
    auto& owner = State().owner(target);
    owner.require(target.node);
    domain::NodeId parent = 0, before = 0;
    if (mode == InsertMode::Append) { parent = target.node; }
    else if (mode == InsertMode::Prepend) {
      parent = target.node; before = owner.require(target.node).first_child_id();
    } else {
      parent = owner.require(target.node).parent_id();
      if (!parent) return;  // ChildNode methods on a parentless node are no-ops.
      if (mode == InsertMode::Before || mode == InsertMode::ReplaceWith) before = target.node;
      else {
        const auto siblings = owner.require(parent).child_ids();
        const size_t index = size_t(owner.require(parent).index_of_child(target.node));
        before = index + 1 < siblings.size() ? siblings[index + 1] : 0;
      }
    }
    // Validate every item and reserve every Text before the first mutation, so
    // a rejected item leaves the published tree unchanged.
    const bool document = owner.require(parent).node_type() == domain::NodeType::Document;
    size_t elements = 0, texts = 0, units = 0;
    for (const Item& item : items) {
      if (Null(item.node)) { ++texts; units += item.text.size(); continue; }
      // A node from another document of this owner is adopted at insert time;
      // its kind comes from its own document, and it cannot already be an
      // ancestor of the target because it is not in this tree yet.
      const bool foreign = item.node.document != target.document;
      const domain::NodeType type =
          State().owner(item.node).require(item.node.node).node_type();
      if (!foreign && IsAncestor(owner,item.node.node,parent))
        throw domain::DomError(domain::DomErrorCode::HierarchyRequest,"Insertion would create a cycle");
      if (type == domain::NodeType::Document || type == domain::NodeType::Attribute)
        throw domain::DomError(domain::DomErrorCode::HierarchyRequest,"Node is not an allowed child");
      if (document) {
        // A DocumentFragment contributes its own children, so the document
        // element/Text constraints are validated for the whole fragment here,
        // before the first insertion mutates anything.
        if (type == domain::NodeType::DocumentFragment) {
          auto& source = State().owner(item.node);
          for (domain::NodeId child : source.require(item.node.node).child_ids()) {
            const domain::NodeType kind = source.require(child).node_type();
            if (kind == domain::NodeType::Text)
              throw domain::DomError(domain::DomErrorCode::HierarchyRequest,"Document cannot contain Text nodes");
            if (kind == domain::NodeType::Element) ++elements;
          }
        } else {
          if (type == domain::NodeType::Text)
            throw domain::DomError(domain::DomErrorCode::HierarchyRequest,"Document cannot contain Text nodes");
          if (type == domain::NodeType::Element) ++elements;
        }
      }
    }
    if (document && elements > 1)
      throw domain::DomError(domain::DomErrorCode::HierarchyRequest,"Document already has an element");
    State().budget(texts,units);
    NodeKey previous, next;
    if (mode == InsertMode::ReplaceWith || mode == InsertMode::Detach) {
      const auto siblings = owner.require(parent).child_ids();
      const size_t index = size_t(owner.require(parent).index_of_child(target.node));
      before = index + 1 < siblings.size() ? siblings[index + 1] : 0;
      const NodeKey removed_parent = Key(target.document, parent);
      State().Neighbours(removed_parent, target.node, &previous, &next);
      owner.remove_child(parent,target.node);
      if (mode == InsertMode::Detach)
        State().NoteRemoved(removed_parent, {target.node}, previous, next);
    }
    const NodeKey parent_key = Key(target.document, parent);
    std::vector<domain::NodeId> created;
    created.reserve(items.size());
    // One ordered pass: a Text is created here, a node from another document of
    // this owner is adopted and inserted by the one pre-insertion adoption
    // implementation, and a same-document node is inserted in place. Every item
    // was validated above, before this first mutation.
    for (const Item& item : items) {
      if (Null(item.node)) {
        const domain::NodeId text = owner.create_text_node16(item.text);
        created.push_back(text);
        owner.insert_before(parent,text,before);
        continue;
      }
      if (item.node.document != target.document) {
        created.push_back(MoveIn(State().document_key(target.document), item.node,
                                 parent_key, Key(target.document, before), {}, rekey).node);
        continue;
      }
      created.push_back(item.node.node);
      const NodeKey old_parent = Key(item.node.document, owner.require(item.node.node).parent_id());
      owner.insert_before(parent,item.node.node,before);
      if (!Null(old_parent)) State().NoteStyle(old_parent, MutationKind::ChildList);
    }
    if (mode == InsertMode::ReplaceWith)
      State().NoteReplace(parent_key, created, {target.node});
    else
      State().NoteInserted(parent_key, created);
    State().Touch();
  });
}
}
