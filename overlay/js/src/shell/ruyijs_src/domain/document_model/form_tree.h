#pragma once
#include "domain/document_model/document.h"
#include "domain/document_model/input_values.h"

namespace ruyijs::domain::document_model {
inline bool form_tag(const DocumentAggregate& doc, NodeId id, DomStringView name) {
  const auto* e = doc.get_element(id);
  return e && e->tag_name().html() && e->tag_name().local16() == name;
}
inline bool form_control(const Element& e) {
  if (!e.tag_name().html()) return false;
  const auto& tag = e.tag_name().local16();
  return tag == u"input" || tag == u"button" || tag == u"select" || tag == u"textarea" ||
      tag == u"fieldset" || tag == u"object" || tag == u"output";
}
inline NodeId form_owner(const DocumentAggregate& doc, NodeId id) {
  const auto* e = doc.get_element(id);
  if (!e || !form_control(*e)) return 0;
  auto explicit_id = e->get_attribute16(u"form");
  if (explicit_id) {
    if (explicit_id->empty() || doc.root_id(id) != doc.id()) return 0;
    for (auto node : doc.descendants(doc.id())) {
      const auto* candidate = doc.get_element(node);
      if (candidate && candidate->get_attribute16(u"id") == explicit_id)
        return form_tag(doc, node, u"form") ? node : 0;
    }
    return 0;
  }
  for (auto parent = e->parent_id(); parent; parent = doc.require(parent).parent_id())
    if (form_tag(doc, parent, u"form")) return parent;
  return 0;
}
inline std::vector<NodeId> form_nodes(const DocumentAggregate& doc, NodeId form,
                                     const DomStringView* name = nullptr) {
  std::vector<NodeId> out;
  for (auto id : doc.descendants(doc.root_id(form), true)) {
    const auto* e = doc.get_element(id);
    if (!e || form_owner(doc, id) != form) continue;
    if (name && (name->empty() ||
        (e->get_attribute16(u"id").value_or(u"") != *name &&
         e->get_attribute16(u"name").value_or(u"") != *name))) continue;
    out.push_back(id);
  }
  return out;
}
inline bool form_radio(const Element& e) {
  return e.tag_name().html() && e.tag_name().local16() == u"input" &&
      input_type(e) == u"radio";
}
inline DomString radio_value(const DocumentAggregate& doc, const std::vector<NodeId>& group) {
  for (auto id : group) {
    const auto& e = *doc.get_element(id);
    if (form_radio(e) && e.input_checked()) return input_value(e);
  }
  return {};
}
}
