#pragma once
#include <initializer_list>
#include <string_view>

namespace ruyijs::domain::document_model {
inline std::u16string_view reflected_html_direction(std::u16string_view value) noexcept {
  for (std::u16string_view keyword : {std::u16string_view(u"ltr"), std::u16string_view(u"rtl"), std::u16string_view(u"auto")}) {
    if (value.size() != keyword.size()) continue;
    bool matches = true;
    for (size_t index = 0; index < value.size(); ++index) {
      char16_t c = value[index];
      if (c >= u'A' && c <= u'Z') c += u'a' - u'A';
      if (c != keyword[index]) { matches = false; break; }
    }
    if (matches) return keyword;
  }
  return {};
}
}
