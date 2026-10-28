#include "document_model/model_internal.h"
#include "domain/document_model/select_values.h"

namespace ruyijs::document_model {
Status Model::OptionSelected(NodeKey key, bool* out) const noexcept {
  return Protect([&] { *out = State().element(key).option_selected(); });
}
Status Model::SetOptionSelected(NodeKey key, bool value) noexcept {
  return Protect([&] { State().element(key).set_option_selected(value); });
}
Status Model::OptionText(NodeKey key, DomString* out) const noexcept {
  std::optional<DomString> text;
  Status status = TextContent(key, &text);
  if (status.code != ErrorCode::None) return status;
  return Protect([&] { *out = domain::option_text(text.value_or(u"")); });
}
Status Model::SelectSize(NodeKey key, uint32_t* out) const noexcept {
  return Protect([&] {
    *out = domain::select_size(State().element(key).get_attribute16(u"size").value_or(u""));
  });
}
Status Model::SetSelectSize(NodeKey key, uint32_t value) noexcept {
  DomString text;
  Status status = Protect([&] { text = domain::from_utf8(std::to_string(domain::select_size(value))); });
  return status.code == ErrorCode::None ? SetAttribute(key, u"size", text) : status;
}
Status Model::SetSelectAutocomplete(NodeKey key, DomStringView value) noexcept {
  return domain::select_autocomplete(value) ? SetAttribute(key, u"autocomplete", value) : Status{};
}
}
