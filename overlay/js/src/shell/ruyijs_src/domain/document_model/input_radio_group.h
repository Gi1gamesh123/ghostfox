#pragma once
#include "domain/document_model/form_tree.h"
#include "domain/document_model/input_values.h"

namespace ruyijs::domain::document_model {
inline bool input_element(const Element* e) {
  return e && e->tag_name().html() && e->tag_name().local16() == u"input";
}
inline std::vector<NodeId> input_radio_group(const DocumentAggregate& doc, NodeId id) {
  const auto* e = doc.get_element(id);
  if (!e || !form_radio(*e)) return {id};
  const auto name = e->get_attribute_ns16({}, u"name").value_or(u"");
  if (name.empty()) return {id};
  const auto root = doc.root_id(id), owner = form_owner(doc, id);
  std::vector<NodeId> out;
  for (auto key : doc.descendants(root, true)) {
    const auto* peer = doc.get_element(key);
    if (peer && form_radio(*peer) && peer->get_attribute_ns16({}, u"name") == name &&
        form_owner(doc, key) == owner) out.push_back(key);
  }
  return out;
}
inline void input_set_checked(DocumentAggregate& doc, NodeId id, bool value) {
  auto& e = *doc.get_element(id);
  const auto group = input_radio_group(doc, id);
  for (auto peer : group) {
    auto& input = *doc.get_element(peer);
    input.input_check_state().checked = input.input_checked();
    input.input_check_state().dirty = true;
  }
  const bool changed = e.input_checked() != value;
  e.input_check_state().checked = value;
  if (changed && value && form_radio(e))
    for (auto peer : group) if (peer != id) doc.get_element(peer)->input_check_state().checked = false;
}
inline bool input_group_missing(const DocumentAggregate& doc, NodeId id) {
  bool required = false, checked = false;
  for (auto key : input_radio_group(doc, id)) {
    const auto& e = *doc.get_element(key);
    required |= e.has_attribute_ns16({}, u"required");
    checked |= e.input_checked();
  }
  return required && !checked;
}
}
