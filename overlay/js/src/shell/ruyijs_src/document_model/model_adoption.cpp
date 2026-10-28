// Prepare foreign native subtrees using the original transfer clone and budget.
#include "document_model/model_transfer_values.h"
#include <algorithm>

namespace ruyijs::document_model {
bool Model::SetIdentityTransfer(bool pending) noexcept {
  const bool previous = impl_ && impl_->identityTransfer;
  if (impl_) impl_->identityTransfer = pending;
  return previous;
}

Status Model::PrepareAdoption(NodeKey document, const Model& source_model, NodeKey source,
                              AdoptionDraft* result, bool fragment_insertion) noexcept {
  AdoptionDraft prepared;
  Status status = Protect([&] {
    State().document(document);
    std::vector<std::pair<NodeKey, NodeKey>> roots{{source, document}};
    for (size_t i = 0; i < roots.size(); ++i) {
      const NodeKey key = roots[i].first, destination = roots[i].second;
      auto& target = State().document(destination);
      const auto& from = source_model.State().owner(key);
      const auto& node = from.require(key.node);
      if (node.node_type() == domain::NodeType::Document)
        throw domain::DomError(domain::DomErrorCode::NotSupported, "A Document cannot be adopted");
      const bool fragment = i == 0 && fragment_insertion;
      if (node.shadow_host_id() && !fragment)
        throw domain::DomError(domain::DomErrorCode::HierarchyRequest, "A ShadowRoot cannot be adopted");
      const auto written = from.owned_descendants(key.node, true);
      size_t units = 0;
      for (auto id : written) units += from.require(id).storage_units();
      State().budget(written.size(), units);
      // Reserve publication bookkeeping before creating anything it must own.
      prepared.drafts.reserve(prepared.drafts.size() + 1);
      domain::NodeId copy;
      if (fragment) {
        copy = target.create_fragment();
        try {
          for (auto child : node.child_ids())
            target.append_child(copy, domain::DomTreeService::clone_into(from, child, target, true, true));
        } catch (...) { target.remove_node(copy); throw; }
      } else copy = domain::DomTreeService::clone_into(from, key.node, target, true, true);
      const Draft draft = State().remember(Key(destination.document, copy), false);
      prepared.drafts.push_back(draft);
      if (i == 0) prepared.root = draft.root;
      const auto made = target.owned_descendants(copy, true);
      PreserveElementState(from, target, written, made);
      PairIdentities(written, made, key.document, destination.document, &prepared.identities);
      // Template content is an owned detached fragment in the existing Model.
      // Its original reference is migrated together with the host's subtree.
      for (auto id : written) {
        const auto* element = from.get_element(id);
        if (!element) continue;
        const auto ref = element->template_content();
        if (!ref.node) continue;
        const NodeKey fragment{ref.document, ref.node};
        if (std::none_of(roots.begin(), roots.end(), [&](const auto& item) {
              return item.first.document == fragment.document && item.first.node == fragment.node;
            })) roots.push_back({fragment, State().template_document(destination)});
      }
    }
    for (const auto& pair : prepared.identities) {
      const auto* old = source_model.State().owner(pair.first).get_element(pair.first.node);
      if (!old || !old->template_content().node) continue;
      const auto ref = old->template_content();
      for (const auto& content : prepared.identities)
        if (content.first.document == ref.document && content.first.node == ref.node) {
          State().element(pair.second).set_template_content(
              {content.second.document, content.second.node});
          break;
        }
    }
    std::sort(prepared.identities.begin(), prepared.identities.end(), [](const auto& a, const auto& b) {
      return a.first.document != b.first.document ? a.first.document < b.first.document
                                                 : a.first.node < b.first.node;
    });
  });
  if (status.code != ErrorCode::None) {
    for (const Draft& draft : prepared.drafts) Rollback(draft.token);
    return status;
  }
  *result = std::move(prepared);
  return {};
}
}  // namespace ruyijs::document_model
