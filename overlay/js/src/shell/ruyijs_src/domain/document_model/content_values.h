#pragma once
#include "domain/document_model/element.h"
#include <cerrno>
#include <cstdlib>
#include <limits>

namespace ruyijs::domain::document_model {
inline DomString content_attribute(const Element& e, DomStringView name, DomStringView raw, bool writing) {
  const auto& tag = e.tag_name().local16();
  if (tag == u"style" && name == u"type" && raw.empty() && !writing) return u"text/css";
  if (tag == u"ul" && name == u"type") {
    auto value = ascii_lower(raw);
    return value == u"disc" || value == u"circle" || value == u"square" ? value : DomString();
  }
  if (tag == u"p" && name == u"align" && writing) {
    auto value = ascii_lower(raw);
    return value == u"left" || value == u"right" || value == u"center" || value == u"justify" ? value : DomString();
  }
  return DomString(raw);
}
inline int32_t content_integer(const Element& e, DomStringView name) {
  const int32_t fallback = name == u"start" ? 1 : 0;
  const auto text = to_utf8(e.get_attribute16(name).value_or(u""));
  char* end = nullptr; errno = 0;
  const auto value = std::strtoll(text.c_str(), &end, 10);
  if (end == text.c_str() || errno == ERANGE || value < std::numeric_limits<int32_t>::min() ||
      value > std::numeric_limits<int32_t>::max()) return fallback;
  return int32_t(value);
}
}
