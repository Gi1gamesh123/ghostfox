#include "document_model/model_internal.h"
#include "domain/document_model/textarea_values.h"

namespace ruyijs::document_model {
Status Model::TextareaText(NodeKey key, DomStringView name, DomString* out) const noexcept {
  return Protect([&] {
    const auto& e = State().element(key);
    if (name == u"value") *out = domain::textarea_value(State().owner(key), e);
    else if (name == u"defaultValue") *out = State().owner(key).text_content16(key.node);
    else if (name == u"selectionDirection") *out = e.textarea_state().direction_name();
    else {
      *out = e.get_attribute16(name).value_or(u"");
      if (name == u"wrap" && out->empty()) *out = u"soft";
    }
  });
}
Status Model::TextareaNumber(NodeKey key, DomStringView name, double* out) const noexcept {
  return Protect([&] {
    const auto& e = State().element(key);
    if (name == u"selectionStart") *out = e.textarea_state().start;
    else if (name == u"textLength" || name == u"selectionEnd") {
      const auto length = domain::textarea_value(State().owner(key), e).size();
      *out = name == u"textLength" ? double(length) : domain::textarea_end(e, length);
    } else *out = domain::textarea_integer(name, e.get_attribute16(name).value_or(u""));
  });
}
Status Model::TextareaWillValidate(NodeKey key, bool* out) const noexcept {
  return Protect([&] { *out = domain::textarea_will_validate(State().element(key)); });
}
Status Model::SetTextareaValue(NodeKey key, DomStringView text) noexcept {
  return Protect([&] {
    auto& value = State().element(key).textarea_state().value;
    State().budget(0, text.size(), value.size());
    DomString replacement(text); value.swap(replacement);
  });
}
Status Model::SetTextareaSelection(NodeKey key, uint32_t start, uint32_t end,
                                   DomStringView direction, int field) noexcept {
  return Protect([&] {
    auto& e = State().element(key); auto& state = e.textarea_state();
    const auto length = uint32_t(domain::textarea_value(State().owner(key), e).size());
    // 0=start, 1=end, 2=direction, 3=range, 4=select all.
    if (field == 0 || field == 3 || field == 4) state.start = field == 4 ? 0 : std::min(start, length);
    if (field == 1 || field == 3 || field == 4) state.end = field == 4 ? length : std::min(end, length);
    if (field >= 2) state.direction = field == 4 ? domain::TextareaState::Forward
        : domain::TextareaState::parse_direction(direction);
  });
}
Status Model::TextareaRange(NodeKey key, DomStringView replacement, uint32_t start, uint32_t end,
                            bool explicit_range, DomStringView mode) noexcept {
  return Protect([&] {
    auto& e = State().element(key); auto& state = e.textarea_state();
    auto text = domain::textarea_value(State().owner(key), e);
    if (!explicit_range) { start = state.start; end = domain::textarea_end(e, text.size()); }
    if (start > end || end > text.size())
      throw domain::DomError(domain::DomErrorCode::IndexSize, "Textarea range exceeds text");
    const size_t size = text.size() - (end - start) + replacement.size();
    State().budget(0, size, state.value.size());
    text.replace(start, end - start, replacement);
    const uint32_t after = start + uint32_t(replacement.size());
    if (mode == u"select") { state.start = start; state.end = after; }
    else if (mode == u"start") { state.start = start; state.end = start; }
    else if (mode == u"end") { state.start = after; state.end = after; }
    state.value.swap(text);
  });
}
}
