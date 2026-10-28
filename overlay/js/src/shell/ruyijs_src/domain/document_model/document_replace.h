#pragma once
#include "domain/document_model/document.h"

namespace ruyijs::domain::document_model {
// Pre-insertion at the end, before replace-all removes any existing children.
// Source may be another existing document; validation never adopts or mutates.
inline void DocumentAggregate::ensure_append_validity(
    NodeId parent, const DocumentAggregate& source, NodeId child) const {
  const auto& p = require(parent);
  const auto& c = source.require(child);
  if (p.node_type() != NodeType::Document && p.node_type() != NodeType::Element &&
      p.node_type() != NodeType::DocumentFragment)
    throw DomError(DomErrorCode::HierarchyRequest, "Invalid parent node type");
  if (this == &source) validate_parent(parent, child);
  const auto allowed = [](NodeType type) {
    return type == NodeType::Element || type == NodeType::Text || type == NodeType::Comment ||
           type == NodeType::DocumentType || type == NodeType::DocumentFragment ||
           type == NodeType::ProcessingInstruction;
  };
  if (!allowed(c.node_type()))
    throw DomError(DomErrorCode::HierarchyRequest, "Node is not an allowed child");
  const bool document = p.node_type() == NodeType::Document;
  if (!document && c.node_type() == NodeType::DocumentType)
    throw DomError(DomErrorCode::HierarchyRequest, "DocumentType requires a Document parent");
  if (!document) return;
  size_t elements = 0, doctypes = 0;
  const auto count = [&](const Node& node) {
    if (node.node_type() == NodeType::Text)
      throw DomError(DomErrorCode::HierarchyRequest, "Document cannot contain Text children");
    if (node.node_type() == NodeType::Element) ++elements;
    if (node.node_type() == NodeType::DocumentType) ++doctypes;
  };
  if (c.node_type() == NodeType::DocumentFragment) {
    for (NodeId id : c.child_ids()) count(source.require(id));
  } else count(c);
  if (elements > 1 || doctypes > 1 || (elements && doctypes))
    throw DomError(DomErrorCode::HierarchyRequest, "Document child constraints are violated");
  for (NodeId id : p.child_ids()) {
    const auto type = require(id).node_type();
    if ((elements && type == NodeType::Element) ||
        (doctypes && (type == NodeType::Element || type == NodeType::DocumentType)))
      throw DomError(DomErrorCode::HierarchyRequest, "Document already has conflicting children");
  }
}

// The prepared sequence owns all needed vector storage before this commit.
// Nodes remain in the original aggregate and retain their existing identities.
inline void DocumentAggregate::commit_child_list(
    NodeId parent, NodeId child, std::vector<NodeId>& children) {
  auto& p = require(parent);
  auto& c = require(child);
  const bool fragment = c.node_type() == NodeType::DocumentFragment;
  for (NodeId id : children) require(id);
  for (NodeId id : p.child_ids()) require(id).set_parent(0);
  if (fragment) c.clear_children();
  else if (c.parent_id() && c.parent_id() != parent)
    require(c.parent_id()).remove_child_id(child);
  p.replace_children(children);
  for (NodeId id : p.child_ids()) require(id).set_parent(parent);
}
}
