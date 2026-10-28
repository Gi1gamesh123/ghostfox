#pragma once
#include "domain/document_model/input_values.h"

namespace ruyijs::domain::document_model {
// Same bit order as the existing ValidityState prototype, excluding derived valid.
inline uint32_t input_validity(const Element& e, DomString* message, bool radio_missing = false) {
  const auto type = input_type(e), value = input_value(e);
  uint32_t flags = e.custom_validity().empty() ? 0 : 1u << 9;
  const bool toggle = type == u"radio" || type == u"checkbox";
  if (type == u"radio" ? radio_missing :
      e.get_attribute16(u"required") && (toggle ? !e.input_checked() : value.empty())) flags |= 1;
  if (!value.empty()) {
    const auto at = value.find(u'@');
    if (type == u"email" && (at == DomString::npos || at == 0 || value.find(u'.', at) == DomString::npos)) flags |= 2;
    if (type == u"url" && value.find(u"://") == DomString::npos) flags |= 2;
  }
  const auto max = input_integer(u"maxlength", e.get_attribute16(u"maxlength").value_or(u""));
  const auto min = input_integer(u"minlength", e.get_attribute16(u"minlength").value_or(u""));
  if (max >= 0 && double(value.size()) > max) flags |= 1u << 3;
  if (min >= 0 && double(value.size()) < min) flags |= 1u << 4;
  const auto low = e.get_attribute16(u"min").value_or(u""), high = e.get_attribute16(u"max").value_or(u"");
  if (input_numeric(type)) {
    const auto number = input_decimal(value);
    if (number < input_decimal(low)) flags |= 1u << 5;
    if (number > input_decimal(high)) flags |= 1u << 6;
  }
  if (message) {
    if (flags & (1u << 9)) *message = e.custom_validity();
    else if (flags & 1) *message = u"Please fill out this field.";
    else if (flags & 2) *message = type == u"email" ? u"Please enter a valid email address." : u"Please enter a valid URL.";
    else if (flags & (1u << 3)) *message = u"Please shorten this text.";
    else if (flags & (1u << 4)) *message = u"Please lengthen this text.";
    else if (flags & (1u << 5)) *message = u"Value must be greater than or equal to " + low + u".";
    else if (flags & (1u << 6)) *message = u"Value must be less than or equal to " + high + u".";
    else message->clear();
  }
  return flags;
}
}
