#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ruyijs::domain::document_model {
using DomString = std::u16string;
using DomStringView = std::u16string_view;
inline DomString from_utf8(std::string_view text) {
  DomString out;
  for (size_t i = 0; i < text.size();) {
    uint32_t c = static_cast<unsigned char>(text[i++]);
    unsigned tail = 0;
    uint32_t minimum = 0;
    if (c < 0x80) {}
    else if (c >= 0xc2 && c <= 0xdf) { c &= 31; tail = 1; minimum = 0x80; }
    else if (c >= 0xe0 && c <= 0xef) { c &= 15; tail = 2; minimum = 0x800; }
    else if (c >= 0xf0 && c <= 0xf4) { c &= 7; tail = 3; minimum = 0x10000; }
    else throw std::invalid_argument("Invalid UTF-8 DOM string");
    if (tail > text.size() - i) throw std::invalid_argument("Truncated UTF-8 DOM string");
    for (unsigned j = 0; j < tail; ++j) {
      unsigned b = static_cast<unsigned char>(text[i++]);
      if ((b & 0xc0) != 0x80) throw std::invalid_argument("Invalid UTF-8 continuation");
      c = (c << 6) | (b & 63);
    }
    if (c < minimum || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
      throw std::invalid_argument("Invalid Unicode scalar");
    if (c <= 0xffff) out.push_back(char16_t(c));
    else { c -= 0x10000; out.push_back(char16_t(0xd800 + (c >> 10))); out.push_back(char16_t(0xdc00 + (c & 1023))); }
  }
  return out;
}
inline std::string to_utf8(DomStringView text) {
  std::string out;
  for (size_t i = 0; i < text.size(); ++i) {
    uint32_t c = text[i];
    if (c >= 0xd800 && c <= 0xdbff && i + 1 < text.size() &&
        text[i + 1] >= 0xdc00 && text[i + 1] <= 0xdfff)
      c = 0x10000 + ((c - 0xd800) << 10) + (text[++i] - 0xdc00);
    else if (c >= 0xd800 && c <= 0xdfff) throw std::invalid_argument("Unpaired DOM surrogate");
    if (c < 0x80) out.push_back(char(c));
    else if (c < 0x800) { out.push_back(char(0xc0 | (c >> 6))); out.push_back(char(0x80 | (c & 63))); }
    else if (c < 0x10000) {
      out.push_back(char(0xe0 | (c >> 12))); out.push_back(char(0x80 | ((c >> 6) & 63))); out.push_back(char(0x80 | (c & 63)));
    } else {
      out.push_back(char(0xf0 | (c >> 18))); out.push_back(char(0x80 | ((c >> 12) & 63)));
      out.push_back(char(0x80 | ((c >> 6) & 63))); out.push_back(char(0x80 | (c & 63)));
    }
  }
  return out;
}
inline DomString ascii_lower(DomStringView value) {
  DomString out(value); for (auto& c : out) if (c >= u'A' && c <= u'Z') c += 32; return out;
}
inline DomString ascii_upper(DomStringView value) {
  DomString out(value); for (auto& c : out) if (c >= u'a' && c <= u'z') c -= 32; return out;
}
inline bool ascii_space(char16_t c) { return c == 9 || c == 10 || c == 12 || c == 13 || c == 32; }
inline std::vector<DomString> tokens(DomStringView value) {
  std::vector<DomString> out;
  for (size_t i = 0; i < value.size();) {
    while (i < value.size() && ascii_space(value[i])) ++i;
    size_t begin = i;
    while (i < value.size() && !ascii_space(value[i])) ++i;
    if (i != begin) out.emplace_back(value.substr(begin, i - begin));
  }
  return out;
}
inline bool name_start(uint32_t c) {
  return c == ':' || c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
    (c >= 0xc0 && c <= 0xd6) || (c >= 0xd8 && c <= 0xf6) || (c >= 0xf8 && c <= 0x2ff) ||
    (c >= 0x370 && c <= 0x37d) || (c >= 0x37f && c <= 0x1fff) ||
    (c >= 0x200c && c <= 0x200d) || (c >= 0x2070 && c <= 0x218f) ||
    (c >= 0x2c00 && c <= 0x2fef) || (c >= 0x3001 && c <= 0xd7ff) ||
    (c >= 0xf900 && c <= 0xfdcf) || (c >= 0xfdf0 && c <= 0xfffd) ||
    (c >= 0x10000 && c <= 0xeffff);
}
inline bool valid_name(DomStringView name) {
  if (name.empty()) return false;
  for (size_t i = 0; i < name.size(); ++i) {
    bool first = i == 0;
    uint32_t c = name[i];
    if (c >= 0xd800 && c <= 0xdbff && i + 1 < name.size() &&
        name[i + 1] >= 0xdc00 && name[i + 1] <= 0xdfff)
      c = 0x10000 + ((c - 0xd800) << 10) + (name[++i] - 0xdc00);
    if (!name_start(c) && (first || !(c == '-' || c == '.' || (c >= '0' && c <= '9') ||
        c == 0xb7 || (c >= 0x300 && c <= 0x36f) || (c >= 0x203f && c <= 0x2040)))) return false;
  }
  return true;
}
// The selected Firefox donor implements the relaxed DOM local-name algorithms,
// not XML QName validation, for createElement/setAttribute (nsContentUtils.cpp).
inline bool ascii_alpha(char16_t c) { return (c >= u'A' && c <= u'Z') || (c >= u'a' && c <= u'z'); }
inline bool valid_element_local_name(DomStringView name) {
  if (name.empty()) return false;
  if (ascii_alpha(name[0])) {
    for (char16_t c : name.substr(1)) if (!c || ascii_space(c) || c == u'/' || c == u'>') return false;
    return true;
  }
  if (name[0] != u':' && name[0] != u'_' && name[0] < 0x80) return false;
  for (char16_t c : name.substr(1))
    if (!(ascii_alpha(c) || (c >= u'0' && c <= u'9') || c == u'-' || c == u'.' || c == u':' || c == u'_' || c >= 0x80)) return false;
  return true;
}
inline bool valid_attribute_local_name(DomStringView name) {
  if (name.empty()) return false;
  for (char16_t c : name) if (!c || ascii_space(c) || c == u'>' || c == u'/' || c == u'=') return false;
  return true;
}
}
