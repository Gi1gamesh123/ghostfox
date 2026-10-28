// Current input state belongs to the original Element, shared by IDL, native
// editing, validation and form submission. Default-mode writes remain attributes.
#include "document_model/model_internal.h"
#include "domain/document_model/input_radio_membership.h"

namespace ruyijs::document_model {
Status Model::SetInputValue(NodeKey key, DomStringView value) noexcept {
  bool attribute = false;
  const auto status = Protect([&] {
    auto& e = State().element(key);
    const auto type = domain::input_type(e);
    attribute = domain::input_default_mode(type);
    if (attribute) return;
    if (type == u"file") {
      if (!value.empty())
        throw domain::DomError(domain::DomErrorCode::InvalidState,
                              "A file input value may only be set to the empty string");
      e.input_current_value().reset();
      return;
    }
    auto next = domain::input_sanitize(type, value);
    auto& current = e.input_current_value();
    State().budget(0, next.size(), current ? current->size() : 0);
    current = std::move(next);
    State().TouchStyle();
  });
  return status.code == ErrorCode::None && attribute ? SetAttribute(key, u"value", value) : status;
}

void Model::Impl::InputTypeChanged(NodeKey key, const DomString* old_type) {
  auto& e = element(key);
  if (!domain::input_element(&e)) return;
  const auto before = domain::input_type(old_type ? DomStringView(*old_type) : DomStringView{});
  const auto after = domain::input_type(e);
  if (before == after) return;
  auto& current = e.input_current_value();
  const bool was_value = before != u"file" && !domain::input_default_mode(before);
  const bool is_default = domain::input_default_mode(after);
  if (was_value && is_default) {
    const auto old_attribute = e.get_attribute_ns16({}, u"value");
    const auto text = current ? *current : domain::input_sanitize(before, old_attribute.value_or(u""));
    if (!text.empty()) {
      budget(0, text.size() + (old_attribute ? 0 : 5), old_attribute ? old_attribute->size() : 0);
      e.set_attribute16(u"value", text);
      NoteAttributes(key, u"value", {}, old_attribute ? &*old_attribute : nullptr);
    }
  }
  if (after == u"file" || is_default || !was_value) current.reset();
  else if (current) *current = domain::input_sanitize(after, *current);
}

Status Model::ResetInputValues(NodeKey form) noexcept {
  return Protect([&] {
    auto& doc = State().owner(form);
    const auto nodes = domain::form_nodes(doc, form.node);
    for (const auto id : nodes) {
      auto* e = doc.get_element(id);
      if (!domain::input_element(e)) continue;
      e->input_current_value().reset();
      auto& check = e->input_check_state();
      check.checked.reset();
      check.dirty = false;
    }
    for (const auto id : nodes) domain::input_join_radio(doc, id);
    State().TouchStyle();
  });
}
}  // namespace ruyijs::document_model
