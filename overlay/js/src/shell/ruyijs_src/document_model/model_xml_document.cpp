#include "document_model/model_internal.h"
namespace ruyijs::document_model {
Status Model::CreatePublicDocument(DomStringView namespace_uri, DomStringView name, NodeKey doctype,
    Draft* result, std::vector<std::pair<NodeKey, NodeKey>>* rekey) noexcept {
  return Protect([&] {
    auto& state = State();
    const bool has_root = !name.empty();
    const size_t colon = name.find(u':');
    if (colon != DomStringView::npos) name.remove_prefix(colon + 1);
    size_t units = 11 + name.size();  // about:blank and the optional root.
    if (!Null(doctype)) {
      const auto& node = state.owner(doctype).require(doctype.node);
      if (node.node_type() != domain::NodeType::DocumentType)
        throw domain::DomError(domain::DomErrorCode::InvalidState, "Expected a DocumentType");
      units += node.storage_units();
    }
    state.budget(1 + size_t(has_root) + size_t(!Null(doctype)), units);
    const DocumentId identity = state.allocate_document();
    auto document = std::make_unique<domain::DocumentAggregate>(1, domain::DocumentMode::Xml, false);
    document->set_ready_state(domain::ReadyState::Complete);
    document->set_url16(u"about:blank");
    document->metadata().set_xml_media_type(namespace_uri == u"http://www.w3.org/1999/xhtml"
        ? domain::XmlMediaType::Xhtml : namespace_uri == u"http://www.w3.org/2000/svg"
        ? domain::XmlMediaType::Svg : domain::XmlMediaType::Xml);
    NodeKey moved;
    if (!Null(doctype)) {
      const auto copy = domain::DomTreeService::clone_into(state.owner(doctype), doctype.node, *document);
      document->append_child(1, copy);
      moved = {identity, copy};
    }
    if (has_root) document->append_child(1, document->create_element(domain::TagName(DomString(name), true, true), false));
    // Prepare the complete tree and identity pair before changing the source.
    // DocumentType is a leaf, so it needs exactly one original transfer pair.
    if (!Null(doctype)) rekey->reserve(rekey->size() + 1);
    state.documents.emplace(identity, std::move(document));
    Draft draft;
    try { draft = state.remember({identity, 1}, true); }
    catch (...) { state.documents.erase(identity); throw; }
    if (!Null(doctype)) {
      rekey->emplace_back(doctype, moved);
      IdentityTransferScope publication(state.identityTransfer);
      CommitTransfer(doctype, moved);
    }
    *result = draft;
  });
}
}
