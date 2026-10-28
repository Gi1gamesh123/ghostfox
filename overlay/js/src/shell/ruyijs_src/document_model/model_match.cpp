#include "document_model/model_internal.h"
#include "document_model/model_selector.h"

namespace ruyijs::document_model {
Status Model::Matches(NodeKey node, DomStringView selector, bool* result) const noexcept {
  Selector parsed;
  Status status = Selector::Parse(selector,&parsed);
  if (status.code != ErrorCode::None) return status;
  return Protect([&] {
    const auto& owner = State().owner(node);
    if (!owner.get_element(node.node))
      throw domain::DomError(domain::DomErrorCode::InvalidState,"matches needs an Element identity");
    *result = parsed.Matches(owner,node.node,node.node);
  });
}
Status Model::Closest(NodeKey node, DomStringView selector, NodeKey* result) const noexcept {
  Selector parsed;
  Status status = Selector::Parse(selector,&parsed);
  if (status.code != ErrorCode::None) return status;
  return Protect([&] {
    const auto& owner = State().owner(node);
    for (auto current = node.node; current; current = owner.require(current).parent_id())
      if (owner.get_element(current) && parsed.Matches(owner,current,node.node)) {
        *result = Key(node.document,current); return;
      }
    *result = {};
  });
}
Status Model::ElementsByTagNameNS(NodeKey scope, DomStringView namespace_uri, DomStringView local,
                                  std::vector<NodeKey>* result) const noexcept {
  return Protect([&] {
    std::vector<NodeKey> values;
    for (auto node : State().owner(scope).query_ns16(scope.node,namespace_uri,local))
      values.push_back(Key(scope.document,node));
    result->swap(values);
  });
}
Status Model::Normalize(NodeKey node) noexcept {
  return Protect([&] {
    domain::DomTreeService::normalize(State().owner(node),node.node);
    State().Touch();
  });
}
Status Model::CompareDocumentPosition(NodeKey node, NodeKey other, uint32_t* result) const noexcept {
  return Protect([&] {
    if (Null(other)) throw domain::DomError(domain::DomErrorCode::InvalidState,"compareDocumentPosition needs a Node");
    *result = node.document == other.document
        ? domain::DomTreeService::compare_document_position(State().owner(node),node.node,other.node)
        : uint32_t(1 | 32 | 4);  // Disconnected, implementation-specific, following.
  });
}
}
