#pragma once
#include "domain/document_model/document.h"

namespace ruyijs::domain::document_model {
inline bool labelable(const Element& e) {
  if (!e.tag_name().html()) return false;
  const auto& tag = e.tag_name().local16();
  if (tag == u"input") return ascii_lower(e.get_attribute16(u"type").value_or(u"")) != u"hidden";
  return tag == u"button" || tag == u"select" || tag == u"textarea" || tag == u"meter" ||
      tag == u"progress" || tag == u"output";
}
inline NodeId label_control(const DocumentAggregate& doc, const Element& label) {
  const auto target = label.get_attribute16(u"for").value_or(u"");
  for (auto id : doc.descendants(target.empty() ? label.id() : doc.id())) {
    const auto* e = doc.get_element(id);
    if (!e) continue;
    if (target.empty()) { if (labelable(*e)) return id; }
    else if (e->get_attribute16(u"id").value_or(u"") == target) return labelable(*e) ? id : 0;
  }
  return 0;
}
}
