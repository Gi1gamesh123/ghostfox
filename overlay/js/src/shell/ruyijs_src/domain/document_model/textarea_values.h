#pragma once
#include "domain/document_model/document.h"
#include <cstdlib>

namespace ruyijs::domain::document_model {
inline DomString textarea_value(const DocumentAggregate& doc, const Element& e) {
  const auto& value = e.textarea_state().value;
  return value.empty() ? doc.text_content16(e.id()) : value;
}
inline double textarea_integer(DomStringView name, DomStringView text) {
  const bool length = name == u"maxlength" || name == u"minlength";
  const double fallback = length ? -1 : name == u"cols" ? 20 : 2;
  const auto raw = to_utf8(text); char* end = nullptr;
  const auto value = std::strtoll(raw.c_str(), &end, 10);
  if (end == raw.c_str() || value < INT32_MIN || value > INT32_MAX) return fallback;
  return length ? double(std::max<int64_t>(-1, value)) : double(std::max<int64_t>(1, value));
}
inline uint32_t textarea_end(const Element& e, size_t length) {
  const auto& state = e.textarea_state();
  return state.start == 0 && state.end == 0 ? uint32_t(length) : state.end;
}
inline bool textarea_will_validate(const Element& e) {
  return !e.get_attribute16(u"disabled") && !e.get_attribute16(u"readonly");
}
inline uint32_t textarea_validity(const DocumentAggregate& doc, const Element& e, DomString* message) {
  const auto value = textarea_value(doc, e);
  uint32_t flags = e.custom_validity().empty() ? 0 : 1u << 9;
  if (e.get_attribute16(u"required") && value.empty()) flags |= 1;
  const auto max = textarea_integer(u"maxlength", e.get_attribute16(u"maxlength").value_or(u""));
  const auto min = textarea_integer(u"minlength", e.get_attribute16(u"minlength").value_or(u""));
  if (max >= 0 && double(value.size()) > max) flags |= 1u << 3;
  if (min >= 0 && !value.empty() && double(value.size()) < min) flags |= 1u << 4;
  if (message) *message = e.custom_validity();
  return flags;
}
}
