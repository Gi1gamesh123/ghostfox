#include "document_model/model_internal.h"

namespace ruyijs::document_model {
NodeKey Model::Impl::template_document(NodeKey key) {
  document(key);
  auto found = templateDocuments.find(key.document);
  if (found != templateDocuments.end()) return document_key(found->second);
  budget(1, 11);
  const DocumentId identity = allocate_document();
  auto inert = std::make_unique<domain::DocumentAggregate>(1, domain::DocumentMode::Html);
  inert->set_ready_state(domain::ReadyState::Complete);
  inert->set_url16(u"about:blank");
  documents.emplace(identity, std::move(inert));
  try {
    templateDocuments.emplace(key.document, identity);
    templateDocuments.emplace(identity, identity);
  } catch (...) {
    templateDocuments.erase(key.document);
    templateDocuments.erase(identity);
    documents.erase(identity);
    throw;
  }
  return document_key(identity);
}
NodeKey Model::Impl::template_content(NodeKey key) {
  auto& host = element(key);
  const auto ref = host.template_content();
  if (ref.node) return {ref.document, ref.node};
  const NodeKey inert = template_document(document_key(key.document));
  budget(1, 0);
  const NodeKey fragment{inert.document, document(inert).create_fragment()};
  host.set_template_content({fragment.document, fragment.node});
  return fragment;
}
namespace {
// Template elements under `root` (itself included) paired with their copies:
// both trees have the same shape, so tree order pairs them.
template <class Visit>
void PairTemplates(const domain::DocumentAggregate& from, domain::NodeId a,
                   const domain::DocumentAggregate& to, domain::NodeId b, Visit visit) {
  const auto written = from.descendants(a, true), made = to.descendants(b, true);
  for (size_t i = 0; i < written.size() && i < made.size(); ++i) {
    const auto* element = from.get_element(written[i]);
    if (element && element->template_content().node && to.get_element(made[i])) visit(*element, made[i]);
  }
}
}  // namespace
void Model::Impl::MoveTemplateChildren(NodeKey key) {
  auto& owner = this->owner(key);
  const auto children = owner.require(key.node).child_ids();
  if (children.empty()) return;
  const NodeKey content = template_content(key);
  auto& inert = document(document_key(content.document));
  // Parser-made nodes have no wrapper yet, so the move is a copy into the inert
  // document that keeps each nested template's content, then a removal.
  for (auto child : children) {
    const auto copy = domain::DomTreeService::clone_into(owner, child, inert, true, true);
    inert.append_child(content.node, copy);
    PairTemplates(owner, child, inert, copy, [&](const domain::Element& from, domain::NodeId made) {
      inert.get_element(made)->set_template_content(from.template_content());
    });
    owner.remove_node(child);
  }
}
void Model::Impl::CloneTemplateContents(const Impl& source, NodeKey from, NodeKey to) {
  const auto& origin = source.owner(from);
  auto& target = owner(to);
  PairTemplates(origin, from.node, target, to.node, [&](const domain::Element& element, domain::NodeId made) {
    const auto ref = element.template_content();
    const auto& inert = source.owner({ref.document, ref.node});
    const auto children = inert.require(ref.node).child_ids();
    if (children.empty()) return;
    const NodeKey content = template_content(Key(to.document, made));
    auto& destination = owner(content);
    for (auto child : children) {
      budget(inert.descendants(child, true).size(), 0);
      const auto copy = domain::DomTreeService::clone_into(inert, child, destination, true);
      destination.append_child(content.node, copy);
      CloneTemplateContents(source, {ref.document, child}, {content.document, copy});
    }
  });
}
DomString Model::Impl::serialize(NodeKey key, bool outer) const {
  const domain::DomTreeService::ContentResolver contents = [this](uint64_t id) -> const domain::DocumentAggregate* {
    auto found = documents.find(id);
    return found == documents.end() ? nullptr : found->second.get();
  };
  const auto& doc = owner(key);
  if (outer) return domain::DomTreeService::serialize_outer16(doc, key.node, &contents);
  // A template's inner markup is its content's.
  const auto* host = doc.get_element(key.node);
  if (host && host->tag_name().html() && host->tag_name().local16() == u"template" && host->template_content().node) {
    const auto ref = host->template_content();
    return domain::DomTreeService::serialize_inner16(owner({ref.document, ref.node}), ref.node, &contents);
  }
  return domain::DomTreeService::serialize_inner16(doc, key.node, &contents);
}
Status Model::TemplateDocument(NodeKey document, NodeKey* result) noexcept {
  return Protect([&] { *result = State().template_document(document); });
}
Status Model::TemplateContent(NodeKey key, NodeKey* out) const noexcept {
  return Protect([&] {
    const auto ref = State().element(key).template_content();
    *out = {ref.document, ref.node};
  });
}
Status Model::BindTemplateContent(NodeKey key, NodeKey fragment) noexcept {
  return Protect([&] {
    if (State().owner(fragment).require(fragment.node).node_type() != domain::NodeType::DocumentFragment)
      throw domain::DomError(domain::DomErrorCode::InvalidState, "Template content requires a fragment");
    auto& element = State().element(key);
    if (!element.tag_name().html() || element.tag_name().local16() != u"template")
      throw domain::DomError(domain::DomErrorCode::InvalidState, "Template content requires a template");
    element.set_template_content({fragment.document, fragment.node});
  });
}
Status Model::DocumentLinks(std::vector<std::pair<NodeKey, NodeKey>>* links) const noexcept {
  return Protect([&] {
    for (const auto& [id, document] : State().documents) {
      const auto type = ParsedType(id, *document);
      if (!Null(type)) links->push_back({Key(id, document->id()), type});
      std::vector<std::pair<domain::NodeId, domain::TemplateContentRef>> entries;
      document->template_links(&entries);
      for (const auto& [host, ref] : entries)
        links->push_back({Key(id, host), {ref.document, ref.node}});
    }
  });
}
void Model::RekeyDocumentReferences(const std::vector<std::pair<NodeKey, NodeKey>>& pairs) noexcept {
  if (!impl_) return;
  for (const auto& [from, to] : pairs)
    for (auto& [id, document] : impl_->documents) {
      document->rekey_template_content({from.document, from.node}, {to.document, to.node});
      const auto type = ParsedType(id, *document);
      if (type.document == from.document && type.node == from.node)
        document->set_parsed_doctype({to.document, to.node});
    }
}
}
