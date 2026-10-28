#pragma once
#include <cstdint>
#include <string_view>

namespace ruyijs::domain::document_model {
inline int32_t tab_index_value(std::u16string_view text) {
  size_t index = 0;
  while (index < text.size() && (text[index] == u' ' || text[index] == u'\t' ||
      text[index] == u'\r' || text[index] == u'\n' || text[index] == u'\f')) ++index;
  bool negative = index < text.size() && text[index] == u'-';
  if (index < text.size() && (text[index] == u'+' || negative)) ++index;
  const size_t first = index;
  int64_t value = 0;
  while (index < text.size() && text[index] >= u'0' && text[index] <= u'9') {
    value = value * 10 + (text[index++] - u'0');
    if (value > INT64_C(2147483647) + int(negative)) return -1;
  }
  return index == first ? -1 : int32_t(negative ? -value : value);
}
}
