#pragma once
#include "domain/document_model/dom_string.h"

namespace ruyijs::domain::document_model {
// The public ruyijs setter accepts these exact tokens; direct attributes stay raw.
inline bool select_autocomplete(DomStringView value) {
  static constexpr DomStringView tokens[] = {
    u"",u"on",u"off",u"name",u"honorific-prefix",u"given-name",u"additional-name",u"family-name",
    u"honorific-suffix",u"nickname",u"email",u"username",u"new-password",u"current-password",u"one-time-code",
    u"organization-title",u"organization",u"street-address",u"address-line1",u"address-line2",
    u"address-line3",u"address-level4",u"address-level3",u"address-level2",u"address-level1",
    u"country",u"country-name",u"postal-code",u"cc-name",u"cc-given-name",u"cc-additional-name",
    u"cc-family-name",u"cc-number",u"cc-exp",u"cc-exp-month",u"cc-exp-year",u"cc-csc",u"cc-type",
    u"transaction-currency",u"transaction-amount",u"language",u"bday",u"bday-day",u"bday-month",
    u"bday-year",u"sex",u"url",u"photo",u"tel",u"tel-country-code",u"tel-national",u"tel-area-code",
    u"tel-local",u"tel-extension",u"impp"
  };
  for (auto token : tokens) if (token == value) return true;
  return false;
}
inline bool option_space(char16_t c) {
  return c == u' ' || c == u'\t' || c == u'\n' || c == u'\r' || c == u'\f';
}
inline DomString option_text(DomStringView source) {
  DomString out; bool space = false;
  for (auto c : source) {
    if (option_space(c)) { space = !out.empty(); continue; }
    if (space) out += u' ';
    out += c; space = false;
  }
  return out;
}
inline uint32_t select_size(uint32_t value) {
  return value < 1 ? 1 : value > 1000 ? 1000 : value;
}
inline uint32_t select_size(DomStringView text) {
  size_t i = 0; while (i < text.size() && option_space(text[i])) ++i;
  bool negative = i < text.size() && text[i] == u'-';
  if (i < text.size() && (text[i] == u'+' || text[i] == u'-')) ++i;
  uint32_t value = 0;
  for (; i < text.size() && text[i] >= u'0' && text[i] <= u'9'; ++i) {
    uint32_t digit = uint32_t(text[i] - u'0');
    if (value > (UINT32_MAX - digit) / 10) return 1;
    value = value * 10 + digit;
  }
  return select_size(negative ? uint32_t(0) - value : value);
}
}
