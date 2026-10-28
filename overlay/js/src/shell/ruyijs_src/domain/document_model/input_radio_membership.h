#pragma once
#include "domain/document_model/input_radio_group.h"

namespace ruyijs::domain::document_model {
inline void input_join_radio(DocumentAggregate& doc, NodeId id,
                             const std::vector<NodeId>& waiting = {}) {
  auto* e = doc.get_element(id);
  if (!e || !form_radio(*e)) return;
  const auto group = input_radio_group(doc, id);
  const auto available = [&](NodeId peer) {
    return peer != id && std::find(waiting.begin(), waiting.end(), peer) == waiting.end();
  };
  const bool select = e->input_checked() && (form_owner(doc, id) || doc.composed_root_id(id) == doc.id());
  bool copied = false;
  for (auto peer : group) {
    if (!available(peer)) continue;
    auto& state = doc.get_element(peer)->input_check_state();
    if (select) state.checked = false;
    if (!copied) {
      e->input_check_state().checked = e->input_checked();
      e->input_check_state().dirty = state.dirty; copied = true;
    }
  }
}
inline void input_radio_attribute(DocumentAggregate& doc, NodeId id, DomStringView name,
                                  DomStringView space, const DomString* previous) {
  if (!space.empty()) return;
  auto* e = doc.get_element(id);
  if (!e) return;
  if (input_element(e)) {
    if (name == u"checked" && !e->input_check_state().dirty) {
      const bool before = e->input_check_state().checked.value_or(previous != nullptr);
      e->input_check_state().checked.reset();
      if (e->input_checked() && !before && form_radio(*e)) {
        for (auto peer : input_radio_group(doc, id))
          if (peer != id) doc.get_element(peer)->input_check_state().checked = false;
      }
    } else if (name == u"type" || name == u"name" || name == u"form") {
      input_join_radio(doc, id);
    }
  }
  if (name == u"id") {
    for (auto key : doc.descendants(doc.id())) {
      const auto* peer = doc.get_element(key);
      if (peer && form_radio(*peer) && peer->has_attribute_ns16({}, u"form")) input_join_radio(doc, key);
    }
  }
}
inline void input_radio_inserted(DocumentAggregate& doc, const std::vector<NodeId>& roots) {
  std::vector<NodeId> pending;
  for (auto root : roots) for (auto key : doc.owned_descendants(root, true))
    if (const auto* e = doc.get_element(key); e && form_radio(*e)) pending.push_back(key);
  for (size_t index = 0; index < pending.size(); ++index) {
    std::vector<NodeId> waiting(pending.begin() + index + 1, pending.end());
    input_join_radio(doc, pending[index], waiting);
  }
  // An inserted form/id can resolve an external input's form attribute.
  bool forms = false;
  for (auto root : roots) for (auto key : doc.descendants(root, true))
    if (form_tag(doc, key, u"form")) forms = true;
  if (forms) for (auto key : doc.descendants(doc.id())) {
    const auto* peer = doc.get_element(key);
    if (peer && form_radio(*peer) && peer->has_attribute_ns16({}, u"form")) input_join_radio(doc, key);
  }
}
}
