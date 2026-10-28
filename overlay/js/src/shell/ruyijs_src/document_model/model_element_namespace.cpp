#include "document_model/model_internal.h"
#include "domain/document_model/qualified_element_name.h"

namespace ruyijs::document_model {
Status Model::CreateElementNS(NodeKey document, DomStringView namespace_uri,
                              DomStringView qualified_name, const std::optional<DomString>& is,
                              Draft* result) noexcept {
  return Protect([&] {
    auto& owner = State().document(document);
    State().budget(1, namespace_uri.size() + qualified_name.size() + (is ? is->size() : 0));
    const auto tag = domain::qualified_element_name(namespace_uri, qualified_name, owner.uses_html_syntax());
    const auto custom = tag.html() && is ? std::make_shared<const DomString>(*is) : nullptr;
    const auto node = owner.create_element(tag, false);
    owner.get_element(node)->set_is_value(custom);
    *result = State().remember(Key(document.document, node), false);
  });
}
}
