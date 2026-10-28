// Window name matching over the existing document tree; no parallel index.
#include "document_model/model_internal.h"

namespace ruyijs::document_model {
Status Model::WindowNamed(NodeKey root, DomStringView name,
                          std::vector<NodeKey>* nodes) const noexcept {
  return Protect([&] {
    nodes->clear();
    if (name.empty()) return;
    const auto& owner = State().owner(root);
    for (auto id : owner.descendants(root.node)) {
      const auto* element = owner.get_element(id);
      if (!element || !element->tag_name().html()) continue;
      const auto tag = element->tag_name().local16();
      if (tag != u"embed" && tag != u"form" && tag != u"img" && tag != u"object") continue;
      const auto value = element->get_attribute16(u"name");
      if (value && DomStringView(*value) == name) nodes->push_back(Key(root.document, id));
    }
  });
}
}
