#pragma once
#include "domain/document_model/node.h"

namespace ruyijs::domain::document_model {
struct ShadowOptions {
  bool closed = false, delegates_focus = false, clonable = false;
  bool serializable = false, manual_slots = false;
};
// A real node in DocumentAggregate's sole node table. Its host is an ownership
// edge, never a parent/child tree edge, so light-tree reads remain isolated.
class ShadowRootNode final : public Node {
 public:
  ShadowRootNode(NodeId id, NodeId host, ShadowOptions options)
      : Node(id, NodeType::DocumentFragment), host_(host), options_(options) {}
  NodeId shadow_host_id() const noexcept override { return host_; }
  const ShadowOptions& options() const noexcept { return options_; }
 private:
  NodeId host_;
  const ShadowOptions options_;
};
}
