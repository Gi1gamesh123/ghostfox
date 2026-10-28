#pragma once
#include "domain/document_model/element.h"
#include <cerrno>
#include <cstdlib>

namespace ruyijs::domain::document_model {
inline std::optional<DomString> image_attribute(const Element& image, DomStringView name) {
  const auto raw = image.get_attribute16(name);
  if (name == u"crossorigin" && !raw) return std::nullopt;
  const auto value = ascii_lower(raw.value_or(u""));
  if (name == u"crossorigin") return value == u"use-credentials" ? value : DomString(u"anonymous");
  if (name == u"decoding") return value == u"async" || value == u"sync" ? value : DomString(u"auto");
  if (name == u"loading") return value == u"lazy" || value == u"eager" ? value : DomString(u"auto");
  if (name == u"fetchpriority") return value == u"high" || value == u"low" ? value : DomString(u"auto");
  if (name == u"referrerpolicy") {
    for (auto allowed : {u"no-referrer", u"no-referrer-when-downgrade", u"origin",
                         u"origin-when-cross-origin", u"unsafe-url"})
      if (value == allowed) return value;
    return DomString();
  }
  return raw.value_or(u"");
}
inline uint32_t image_spacing(const Element& image, DomStringView name) {
  const auto raw = to_utf8(image.get_attribute16(name).value_or(u""));
  char* end = nullptr; errno = 0;
  const auto number = std::strtoll(raw.c_str(), &end, 10);
  if (end == raw.c_str() || errno == ERANGE || number > int64_t(UINT32_MAX) ||
      number < -int64_t(UINT32_MAX)) return 0;
  return uint32_t(number);
}
}
