#pragma once
#include "domain/document_model/document.h"

namespace ruyijs::domain::document_model {
inline DomString DocumentAggregate::text_content16(NodeId key) const {
  const auto& node = require(key);
  if (node.node_type() == NodeType::Text || node.node_type() == NodeType::Comment ||
      node.node_type() == NodeType::ProcessingInstruction) return node.text_content16();
  DomString result;
  for (NodeId child : descendants(key)) if (require(child).node_type() == NodeType::Text) result += require(child).text_content16();
  return result;
}
inline void DocumentAggregate::set_text_content16(NodeId key, DomStringView value) {
  auto& node = require(key);
  if (node.node_type() == NodeType::Document) return;
  if (node.node_type() == NodeType::Text || node.node_type() == NodeType::Comment ||
      node.node_type() == NodeType::ProcessingInstruction) {
    node.set_node_value16(DomString(value)); return;
  }
  NodeId text = 0;
  std::vector<NodeId> replacement;
  if (!value.empty()) {
    replacement.reserve(1);
    text = create_text_node16(value);
    replacement.push_back(text);
  }
  for (NodeId child : node.child_ids()) require(child).set_parent(0);
  node.replace_children(replacement);
  if (text) require(text).set_parent(key);
}
}
