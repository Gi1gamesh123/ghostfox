#pragma once
#include "domain/document_model/element.h"
#include <cerrno>
#include <cstdlib>
#include <limits>

namespace ruyijs::domain::document_model {
inline double embedded_integer(const Element& e, DomStringView name) {
  const bool source = e.tag_name().local16() == u"source";
  const bool loop = name == u"loop";
  const double fallback = loop ? -1 : name == u"scrollamount" ? 6 : name == u"scrolldelay" ? 85 : 0;
  const auto text = to_utf8(e.get_attribute16(name).value_or(u""));
  char* end = nullptr; errno = 0;
  const auto value = std::strtoll(text.c_str(), &end, 10);
  if (end == text.c_str() || (!source && *end)) return fallback;
  if (loop) return value > 0 ? double(std::min<int64_t>(value, INT32_MAX)) : -1;
  if (errno == ERANGE || value > int64_t(UINT32_MAX) || value < -int64_t(UINT32_MAX)) return fallback;
  const auto number = uint32_t(value);
  if (!source && number > uint32_t(INT32_MAX)) return fallback;
  return number;
}
inline void validate_embedded_integer(const Element& e, DomStringView name, int64_t value) {
  if (e.tag_name().local16() == u"marquee" && name == u"loop" && value != -1 && value <= 0)
    throw DomError(DomErrorCode::IndexSize, "Marquee loop must be -1 or positive");
}
}
