// Firefox HTMLAllCollection's name matching over the original document tree.
#include "document_model/model_internal.h"
#include <algorithm>

namespace ruyijs::document_model {
Status Model::AllNamed(NodeKey root, const DomStringView* name,
                       std::vector<NodeKey>* nodes, std::vector<DomString>* names) const noexcept {
  return Protect([&] {
    const auto& owner = State().owner(root);
    if (nodes) nodes->clear();
    if (names) names->clear();
    for (auto id : owner.descendants(root.node)) {
      const auto* element = owner.get_element(id);
      if (!element) continue;
      bool eligible = false;
      if (element->tag_name().html()) {
        for (DomStringView tag : {u"a",u"button",u"embed",u"form",u"iframe",u"img",u"input",
                                  u"map",u"meta",u"object",u"select",u"textarea",u"frame",u"frameset"})
          if (element->tag_name().local16() == tag) { eligible = true; break; }
      }
      bool matches = false;
      for (DomStringView attr : {u"id",u"name"}) {
        if (attr == u"name" && !eligible) continue;
        auto value = element->get_attribute16(attr);
        if (!value || value->empty()) continue;
        if (name && DomStringView(*value) == *name) matches = true;
        if (names && std::find(names->begin(), names->end(), *value) == names->end())
          names->push_back(*value);
      }
      if (matches && nodes) nodes->push_back(Key(root.document,id));
    }
  });
}
}
