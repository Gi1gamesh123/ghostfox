#ifndef RUYIJS_DOMAIN_DOCUMENT_MODEL_XML_READER_H
#define RUYIJS_DOMAIN_DOCUMENT_MODEL_XML_READER_H
// Character-level reading for the XML parser (xml_parser.h): positions, XML
// 1.0 names, whitespace and character references. Errors carry expat's message
// and the UTF-16 index Firefox 158 reports (artifacts/xml-ns/more). Pure: no
// engine or DOM dependency.
#include <cstdint>
#include <string>
#include <string_view>

namespace ruyijs::domain::document_model {
struct XmlError {
  std::u16string message;  // expat text, e.g. u"not well-formed"
  size_t index = 0;        // position in the line-normalized input
};
class XmlReader {
 public:
  // Line ends are normalized first (CRLF and lone CR become LF), as expat does.
  explicit XmlReader(std::u16string_view input) {
    text_.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
      if (input[i] == u'\r') {
        text_.push_back(u'\n');
        if (i + 1 < input.size() && input[i + 1] == u'\n') ++i;
      } else {
        text_.push_back(input[i]);
      }
    }
  }
  const std::u16string& text() const noexcept { return text_; }
  size_t pos = 0;
  bool done() const noexcept { return pos >= text_.size(); }
  char16_t peek(size_t ahead = 0) const noexcept {
    return pos + ahead < text_.size() ? text_[pos + ahead] : u'\0';
  }
  bool starts(std::u16string_view literal) const noexcept {
    return text_.compare(pos, literal.size(), literal) == 0;
  }
  [[noreturn]] static void Fail(const char16_t* message, size_t index) {
    throw XmlError{message, index};
  }
  static bool Space(char16_t c) noexcept { return c == u' ' || c == u'\t' || c == u'\n'; }
  bool SkipSpace() noexcept {
    const size_t start = pos;
    while (!done() && Space(text_[pos])) ++pos;
    return pos != start;
  }
  // XML 1.0 (5th edition) NameStartChar / NameChar; a surrogate pair counts
  // as one supplementary character.
  static bool NameStart(char32_t c) noexcept {
    return c == u':' || c == u'_' || (c >= u'A' && c <= u'Z') || (c >= u'a' && c <= u'z') ||
           (c >= 0xC0 && c <= 0xD6) || (c >= 0xD8 && c <= 0xF6) || (c >= 0xF8 && c <= 0x2FF) ||
           (c >= 0x370 && c <= 0x37D) || (c >= 0x37F && c <= 0x1FFF) || (c >= 0x200C && c <= 0x200D) ||
           (c >= 0x2070 && c <= 0x218F) || (c >= 0x2C00 && c <= 0x2FEF) || (c >= 0x3001 && c <= 0xD7FF) ||
           (c >= 0xF900 && c <= 0xFDCF) || (c >= 0xFDF0 && c <= 0xFFFD) || (c >= 0x10000 && c <= 0xEFFFF);
  }
  static bool NameChar(char32_t c) noexcept {
    return NameStart(c) || c == u'-' || c == u'.' || (c >= u'0' && c <= u'9') || c == 0xB7 ||
           (c >= 0x300 && c <= 0x36F) || (c >= 0x203F && c <= 0x2040);
  }
  // The code point at pos and its length in UTF-16 units.
  char32_t Point(size_t* units) const noexcept {
    const char16_t c = peek();
    if (c >= 0xD800 && c <= 0xDBFF) {
      const char16_t low = peek(1);
      if (low >= 0xDC00 && low <= 0xDFFF) {
        *units = 2;
        return 0x10000 + ((char32_t(c) - 0xD800) << 10) + (low - 0xDC00);
      }
    }
    *units = 1;
    return c;
  }
  // A Name; an invalid first character is "not well-formed" at that character.
  std::u16string Name() {
    size_t units = 0;
    if (done() || !NameStart(Point(&units))) Fail(u"not well-formed", pos);
    const size_t start = pos;
    pos += units;
    while (!done() && NameChar(Point(&units))) pos += units;
    return text_.substr(start, pos - start);
  }
  // `&#...;` at pos (pos on '&'); appends the character.
  void CharacterReference(std::u16string* out) {
    const size_t at = pos;
    pos += 2;
    const bool hex = peek() == u'x';
    if (hex) ++pos;
    uint32_t value = 0;
    size_t digits = 0;
    for (; !done() && peek() != u';'; ++pos, ++digits) {
      const char16_t c = peek();
      const int digit = c >= u'0' && c <= u'9' ? c - u'0'
          : hex && c >= u'a' && c <= u'f' ? c - u'a' + 10 : hex && c >= u'A' && c <= u'F' ? c - u'A' + 10 : -1;
      if (digit < 0) Fail(u"not well-formed", pos);
      value = value * (hex ? 16 : 10) + uint32_t(digit);
      if (value > 0x10FFFF) value = 0x110000;
    }
    if (done()) Fail(u"unclosed token", at);
    ++pos;
    const bool valid = digits && (value == 0x9 || value == 0xA || value == 0xD ||
        (value >= 0x20 && value <= 0xD7FF) || (value >= 0xE000 && value <= 0xFFFD) ||
        (value >= 0x10000 && value <= 0x10FFFF));
    if (!valid) Fail(u"reference to invalid character number", at);
    if (value >= 0x10000) {
      value -= 0x10000;
      out->push_back(char16_t(0xD800 + (value >> 10)));
      out->push_back(char16_t(0xDC00 + (value & 0x3FF)));
    } else {
      out->push_back(char16_t(value));
    }
  }

 private:
  std::u16string text_;
};
}  // namespace ruyijs::domain::document_model
#endif
