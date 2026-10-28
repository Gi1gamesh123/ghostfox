#pragma once
#include "domain/document_model/node.h"

namespace ruyijs::domain::document_model {
// A parsed type retains identifiers. The public JS factory supplies empty IDs.
struct ParsedDoctypeRef {
  uint64_t document = 0;  // Zero denotes this DocumentAggregate's identity space.
  NodeId node = 0;
};
class DocumentTypeNode final : public Node {
 public:
  DocumentTypeNode(NodeId id, DomStringView name, bool cloneable,
                   DomStringView public_id = {}, DomStringView system_id = {})
      : Node(id, NodeType::DocumentType), name_(ascii_lower(name)),
        public_id_(public_id), system_id_(system_id), cloneable_(cloneable) {}
  DomString node_name16() const override { return name_; }
  size_t storage_units() const noexcept override { return name_.size() + public_id_.size() + system_id_.size(); }
  bool cloneable() const noexcept { return cloneable_; }
  const DomString& public_id() const noexcept { return public_id_; }
  const DomString& system_id() const noexcept { return system_id_; }
 private:
  DomString name_, public_id_, system_id_;
  bool cloneable_;
};
}
