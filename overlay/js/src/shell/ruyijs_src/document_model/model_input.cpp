#include "document_model/model_internal.h"
#include "domain/document_model/input_validity.h"
#include "domain/document_model/input_radio_group.h"
#include "domain/document_model/textarea_values.h"
#include <charconv>
#include <cmath>

namespace ruyijs::document_model {
Status Model::InputText(NodeKey key, DomStringView name, DomString* out) const noexcept {
  return Protect([&] { *out = domain::input_text(State().element(key), name); });
}
Status Model::InputNumber(NodeKey key, DomStringView name, double* out) const noexcept {
  return Protect([&] {
    const auto& e = State().element(key);
    if (name == u"valueAsNumber") {
      *out = domain::input_numeric(domain::input_type(e)) ? domain::input_decimal(domain::input_value(e))
          : std::numeric_limits<double>::quiet_NaN();
    } else *out = domain::input_integer(name, e.get_attribute16(name).value_or(u""));
  });
}
Status Model::InputTypeValue(DomStringView value, DomString* out) const noexcept {
  return Protect([&] { *out = domain::input_type(value); });
}
Status Model::InputWillValidate(NodeKey key, bool* out) const noexcept {
  return Protect([&] { *out = domain::input_will_validate(State().element(key)); });
}
Status Model::ControlValidation(NodeKey key, uint32_t* flags, DomString* message) const noexcept {
  return Protect([&] {
    const auto& e = State().element(key);
    if (e.tag_name().html() && e.tag_name().local16() == u"input")
      *flags = domain::input_validity(e, message, domain::form_radio(e) &&
          domain::input_group_missing(State().owner(key), key.node));
    else if (e.tag_name().html() && e.tag_name().local16() == u"textarea")
      *flags = domain::textarea_validity(State().owner(key), e, message);
    else {
      *flags = e.custom_validity().empty() ? 0 : 1u << 9;
      if (message) *message = e.custom_validity();
    }
  });
}
Status Model::SetInputNumber(NodeKey key, double value, bool step) noexcept {
  DomString text; bool apply = false;
  auto status = Protect([&] {
    const auto& e = State().element(key);
    apply = domain::input_numeric(domain::input_type(e));
    if (!apply) return;
    if (step) {
      const double old = domain::input_decimal(domain::input_value(e));
      value += std::isnan(old) ? 0 : old;
    }
    if (std::isfinite(value)) {
      char buffer[64];
      const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value);
      if (converted.ec == std::errc{}) text = domain::from_utf8(std::string(buffer, converted.ptr));
    }
  });
  return status.code == ErrorCode::None && apply ? SetInputValue(key, text) : status;
}
Status Model::InputRange(NodeKey key, DomStringView replacement, int32_t start, int32_t end,
                         bool explicit_range) noexcept {
  DomString text; bool apply = false;
  auto status = Protect([&] {
    const auto& e = State().element(key);
    apply = domain::input_textual(domain::input_type(e));
    if (!apply) return;
    text = domain::input_value(e);
    size_t first = explicit_range ? std::min(size_t(uint32_t(start)), text.size()) : 0;
    size_t last = explicit_range ? std::min(size_t(uint32_t(end)), text.size()) : text.size();
    if (first > last) std::swap(first, last);
    State().budget(0, replacement.size(), last - first);
    text.replace(first, last - first, replacement);
  });
  return status.code == ErrorCode::None && apply ? SetInputValue(key, text) : status;
}
}
