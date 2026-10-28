#include "document_model/model_internal.h"

namespace ruyijs::document_model {
Status Model::EventPath(NodeKey key, bool composed,
                        std::vector<NodeKey>* result, std::vector<uint8_t>* flags) const noexcept {
  return Protect([&] {
    const auto& state = State();
    const auto& owner = state.owner(key);
    const auto originalRoot = owner.root_id(key.node);
    std::vector<NodeKey> path;
    std::vector<uint8_t> metadata;
    for (auto node = key.node; node;) {
      if (path.size() >= state.limits.nodes)
        throw domain::DomError(domain::DomErrorCode::LimitExceeded,
                               "Event path exceeds the DOM node limit");
      const auto& value = owner.require(node);
      const auto* shadow = owner.shadow_node(node);
      path.push_back(Key(key.document, node));
      metadata.push_back(shadow && shadow->options().closed ? 1 : 0);
      if (value.parent_id()) node = value.parent_id();
      else if (shadow && (composed || node != originalRoot))
        node = shadow->shadow_host_id();
      else break;
    }
    // The JS adapter may append the owning Window without allocating outside
    // this exception-bounded Model query.
    metadata.reserve(path.size() + 1);
    result->swap(path);
    flags->swap(metadata);
  });
}

Status Model::Retarget(NodeKey key, const Model* againstModel, NodeKey against,
                       NodeKey* result, bool* inShadowTree) const noexcept {
  return Protect([&] {
    const auto& owner = State().owner(key);
    const domain::DocumentAggregate* againstOwner = nullptr;
    if (againstModel && !Null(against)) {
      againstOwner = &againstModel->State().owner(against);
      againstOwner->require(against.node);
    }
    auto current = key.node;
    for (;;) {
      const auto root = owner.root_id(current);
      const auto* shadow = owner.shadow_node(root);
      if (!shadow) break;
      bool inside = false;
      if (againstOwner == &owner) {
        for (auto node = against.node; node;) {
          if (node == root) { inside = true; break; }
          const auto& value = owner.require(node);
          node = value.parent_id() ? value.parent_id() : value.shadow_host_id();
        }
      }
      if (inside) break;
      current = shadow->shadow_host_id();
    }
    *result = Key(key.document, current);
    *inShadowTree = owner.shadow_node(owner.root_id(current)) != nullptr;
  });
}
}
