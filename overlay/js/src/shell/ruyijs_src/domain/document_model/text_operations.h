#pragma once
#include "domain/document_model/document.h"

namespace ruyijs::domain::document_model {
inline const CharacterData& require_text(const DocumentAggregate& owner, NodeId id) {
  const auto& node = owner.require(id);
  if (node.node_type() != NodeType::Text)
    throw DomError(DomErrorCode::InvalidState, "Expected a Text node");
  return static_cast<const CharacterData&>(node);
}
inline DomString whole_text(const DocumentAggregate& owner, NodeId id) {
  const auto& node = require_text(owner, id);
  if (!node.parent_id()) return node.data16();
  const auto& parent = owner.require(node.parent_id());
  const auto& siblings = parent.child_ids();
  size_t begin = size_t(parent.index_of_child(id)), end = begin + 1;
  while (begin && owner.require(siblings[begin - 1]).node_type() == NodeType::Text) --begin;
  while (end < siblings.size() && owner.require(siblings[end]).node_type() == NodeType::Text) ++end;
  DomString value;
  for (size_t i = begin; i < end; ++i) value += require_text(owner, siblings[i]).data16();
  return value;
}
// The original factory creates and roots destination. Allocate both halves
// before committing the tree and swapping text; no duplicate text state owner.
inline void split_text_into(DocumentAggregate& owner, NodeId source, NodeId destination,
                            uint32_t offset) {
  const auto& node = require_text(owner, source);
  const auto& target = require_text(owner, destination);
  if (source == destination || target.parent_id() || target.length())
    throw DomError(DomErrorCode::InvalidState, "Split destination must be an empty detached Text");
  if (offset > node.length())
    throw DomError(DomErrorCode::IndexSize, "Text offset exceeds length");
  DomString head = node.data16().substr(0, offset), tail = node.data16().substr(offset);
  if (node.parent_id()) {
    const auto& parent = owner.require(node.parent_id());
    const auto& siblings = parent.child_ids();
    const size_t position = size_t(parent.index_of_child(source)) + 1;
    const NodeId before = position < siblings.size() ? siblings[position] : 0;
    owner.insert_before(parent.id(), destination, before);
  }
  owner.require(source).set_node_value16(std::move(head));
  owner.require(destination).set_node_value16(std::move(tail));
}
}
