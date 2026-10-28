#include "document_model/model_internal.h"

namespace ruyijs::document_model {
Status Model::ImportNode(NodeKey document, NodeKey source, bool deep, Draft* result) noexcept {
  return ImportNode(document, *this, source, deep, result);
}
Status Model::ImportNode(NodeKey document, const Model& source_model, NodeKey source,
                         bool deep, Draft* result) noexcept {
  return Protect([&] {
    const auto& from = source_model.State().owner(source);
    auto& to = State().document(document);
    const auto& node = from.require(source.node);
    if (node.node_type() == domain::NodeType::Document)
      throw domain::DomError(domain::DomErrorCode::NotSupported, "A Document cannot be imported");
    if (node.shadow_host_id())
      throw domain::DomError(domain::DomErrorCode::NotSupported, "A ShadowRoot cannot be imported");
    const auto written = domain::DomTreeService::cloned_nodes(from, source.node, deep);
    size_t units = 0;
    for (domain::NodeId id : written) units += from.require(id).storage_units();
    State().budget(written.size(), units);
    const domain::NodeId root = domain::DomTreeService::clone_into(from, source.node, to, deep);
    try { if (deep) State().CloneTemplateContents(source_model.State(), source, Key(document.document, root)); }
    catch (...) { to.remove_node(root); throw; }
    *result = State().remember(Key(document.document, root), false);
  });
}
}
