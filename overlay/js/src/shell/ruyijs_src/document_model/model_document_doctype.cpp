#include "document_model/model_internal.h"
namespace ruyijs::document_model {
Status Model::ParsedDocumentType(NodeKey document, NodeKey* result) const noexcept {
  return Protect([&] { *result = ParsedType(document.document, State().document(document)); });
}
Status Model::ParsedDocumentTypeOwners(NodeKey node, std::vector<NodeKey>* result) const noexcept {
  return Protect([&] {
    for (const auto& [id, owner] : State().documents) {
      const auto ref = ParsedType(id, *owner);
      if (ref.document == node.document && ref.node == node.node) result->push_back(Key(id, owner->id()));
    }
  });
}
}
