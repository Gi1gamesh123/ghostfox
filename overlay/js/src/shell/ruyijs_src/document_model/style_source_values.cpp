// Original DOM UTF16/source-title helpers, shared by style and link adapters.
#include "document_model/style_source_values.h"
namespace ruyijs::document_model {
namespace {
void AppendUtf8(std::string* out, char32_t code) {
  if (code <= 0x7F) {
    out->push_back(char(code));
  } else if (code <= 0x7FF) {
    out->push_back(char(0xC0 | (code >> 6)));
    out->push_back(char(0x80 | (code & 0x3F)));
  } else if (code <= 0xFFFF) {
    out->push_back(char(0xE0 | (code >> 12)));
    out->push_back(char(0x80 | ((code >> 6) & 0x3F)));
    out->push_back(char(0x80 | (code & 0x3F)));
  } else {
    out->push_back(char(0xF0 | (code >> 18)));
    out->push_back(char(0x80 | ((code >> 12) & 0x3F)));
    out->push_back(char(0x80 | ((code >> 6) & 0x3F)));
    out->push_back(char(0x80 | (code & 0x3F)));
  }
}
std::string Utf8(DomStringView text) {
  std::string out;
  out.reserve(text.size());
  for (size_t index = 0; index < text.size(); ++index) {
    char16_t unit = text[index];
    char32_t code = unit;
    if (unit >= 0xD800 && unit <= 0xDBFF && index + 1 < text.size() &&
        text[index + 1] >= 0xDC00 && text[index + 1] <= 0xDFFF) {
      code = 0x10000 + ((char32_t(unit) - 0xD800) << 10) + (char32_t(text[index + 1]) - 0xDC00);
      ++index;
    } else if (unit >= 0xD800 && unit <= 0xDFFF) {
      code = 0xFFFD;  // A lone surrogate becomes the replacement character.
    }
    AppendUtf8(&out, code);
  }
  return out;
}
bool Space(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}
std::string Title(const std::string& input) {
  std::string out; bool space = false;
  for (char c : input) {
    if (Space(c)) { space = !out.empty(); continue; }
    if (space) out.push_back(' ');
    space = false; out.push_back(c);
  }
  return out;
}
}
std::string StyleSourceText(DomStringView text) { return Utf8(text); }
DomString StyleSourceWide(const std::string& text) {
  DomString out;
  out.reserve(text.size());
  for (size_t i = 0; i < text.size();) {
    const unsigned char lead = static_cast<unsigned char>(text[i]);
    const size_t length = lead < 0x80 ? 1 : (lead >> 5) == 6 ? 2 : (lead >> 4) == 14 ? 3 : (lead >> 3) == 30 ? 4 : 0;
    char32_t code = length == 1 ? lead : length == 2 ? (lead & 0x1F) : length == 3 ? (lead & 0x0F) : (lead & 0x07);
    bool valid = length != 0 && i + length <= text.size();
    for (size_t k = 1; valid && k < length; ++k) {
      const unsigned char next = static_cast<unsigned char>(text[i + k]);
      valid = (next >> 6) == 2;
      code = (code << 6) | (next & 0x3F);
    }
    if (!valid || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) { out.push_back(u'\uFFFD'); ++i; continue; }
    if (code >= 0x10000) {
      code -= 0x10000;
      out.push_back(char16_t(0xD800 + (code >> 10)));
      out.push_back(char16_t(0xDC00 + (code & 0x3FF)));
    } else {
      out.push_back(char16_t(code));
    }
    i += length;
  }
  return out;
}
bool StyleSourceSpace(char value) { return Space(value); }
std::string StyleSourceTitle(const std::string& text) { return Title(text); }
}
