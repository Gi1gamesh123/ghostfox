#pragma once
#include "domain/document_model/element.h"
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>

namespace ruyijs::domain::document_model {
inline bool view_blank(char16_t c) { return c == u' ' || (c >= u'\t' && c <= u'\r'); }
inline bool view_digit(char16_t c) { return c >= u'0' && c <= u'9'; }
inline bool view_letter(char16_t c) {
  return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z');
}
// Public field extraction, deliberately not a CSS declaration/layout parser.
inline std::optional<int32_t> view_inline_dimension(DomStringView css, DomStringView name) {
  DomString key(name); key.push_back(u':');
  size_t at = css.find(key);
  if (at == DomStringView::npos) return std::nullopt;
  at += key.size();
  while (at < css.size() && view_blank(css[at])) ++at;
  const size_t start = at;
  while (at < css.size() && (view_digit(css[at]) || css[at] == u'.')) ++at;
  const size_t end = at;
  while (at < css.size() && view_letter(css[at])) ++at;
  const auto unit = css.substr(end, at - end);
  if (start == end || (!unit.empty() && unit != u"px")) return std::nullopt;
  std::string number;
  number.reserve(end - start);
  for (size_t i = start; i < end; ++i) number.push_back(char(css[i]));
  double parsed = 0;
  const auto result = std::from_chars(number.data(), number.data() + number.size(), parsed,
                                     std::chars_format::fixed);
  if (result.ec != std::errc() || !std::isfinite(parsed)) return std::nullopt;
  parsed = std::trunc(parsed);
  if (parsed < 0 || parsed > std::numeric_limits<int32_t>::max()) return std::nullopt;
  return int32_t(parsed);
}
inline std::optional<int32_t> view_attribute_dimension(DomStringView text) {
  size_t at = 0;
  while (at < text.size() && view_blank(text[at])) ++at;
  if (at < text.size() && text[at] == u'+') ++at;
  if (at == text.size() || !view_digit(text[at])) return std::nullopt;
  uint32_t number = 0;
  for (; at < text.size() && view_digit(text[at]); ++at) {
    const uint32_t digit = uint32_t(text[at] - u'0');
    if (number > (uint32_t(INT32_MAX) - digit) / 10) return std::nullopt;
    number = number * 10 + digit;
  }
  return int32_t(number);
}
inline int32_t element_view_dimension(const Element& element, bool height) {
  const DomStringView name = height ? u"height" : u"width";
  const auto value = view_inline_dimension(element.get_attribute16(u"style").value_or(u""), name);
  if (value) return *value;
  const auto attribute = view_attribute_dimension(element.get_attribute16(name).value_or(u""));
  if (attribute) return *attribute;
  // Public fixed placeholders, independent of Window/profile and bitmap state.
  const auto& tag = element.tag_name().local16();
  if (tag == u"canvas") return height ? 150 : 300;
  if (tag == u"html" || tag == u"body") return height ? 969 : 1920;
  return 0;
}
}
