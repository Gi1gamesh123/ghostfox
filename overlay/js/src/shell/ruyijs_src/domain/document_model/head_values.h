#pragma once
#include "domain/document_model/element.h"

namespace ruyijs::domain::document_model {
inline DomString head_ascii_lower(DomStringView text) {
  DomString out(text);
  for (auto& c : out) if (c >= u'A' && c <= u'Z') c += u'a' - u'A';
  return out;
}
// Public ruyijs's head element reflections; attributes remain the only state.
inline std::optional<DomString> head_attribute(const Element& e, DomStringView name) {
  auto text = e.get_attribute16(name).value_or(u"");
  if (!e.tag_name().is_script()) {
    if (name == u"crossorigin" && text.empty()) return std::nullopt;
    return text;
  }
  if (name != u"crossorigin" && name != u"referrerpolicy" && name != u"fetchpriority") return text;
  text = head_ascii_lower(text);
  if (name == u"crossorigin") {
    if (text.empty()) return std::nullopt;
    return text == u"use-credentials" ? u"use-credentials" : u"anonymous";
  }
  if (name == u"fetchpriority") return text == u"high" || text == u"low" ? text : DomString(u"auto");
  for (const auto* value : {u"no-referrer", u"no-referrer-when-downgrade", u"origin",
      u"origin-when-cross-origin", u"same-origin", u"strict-origin",
      u"strict-origin-when-cross-origin", u"unsafe-url"})
    if (text == value) return text;
  return std::nullopt;
}
inline bool head_supports_script(DomStringView type) {
  auto text = head_ascii_lower(type);
  return text == u"classic" || text == u"module" || text == u"importmap" || text == u"speculationrules";
}
}
