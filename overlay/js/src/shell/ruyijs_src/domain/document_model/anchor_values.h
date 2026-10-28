#pragma once
#include "domain/document_model/dom_string.h"

namespace ruyijs::domain::document_model {
inline DomString anchor_policy_value(DomStringView raw) {
  size_t first = 0, last = raw.size();
  const auto blank = [](char16_t c) { return c == u' ' || c == u'\t' || c == u'\n' || c == u'\r' || c == u'\f'; };
  while (first < last && blank(raw[first])) ++first;
  while (last > first && blank(raw[last-1])) --last;
  return ascii_lower(raw.substr(first, last-first));
}
inline DomString anchor_policy(DomStringView raw) {
  auto value = anchor_policy_value(raw);
  for (const auto* policy : {u"no-referrer", u"no-referrer-when-downgrade", u"origin",
                           u"origin-when-cross-origin", u"unsafe-url"})
    if (value == policy) return value;
  return {};
}
}
