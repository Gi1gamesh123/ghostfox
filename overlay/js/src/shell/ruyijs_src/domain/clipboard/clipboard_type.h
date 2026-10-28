#ifndef RUYIJS_DOMAIN_CLIPBOARD_CLIPBOARD_TYPE_H
#define RUYIJS_DOMAIN_CLIPBOARD_CLIPBOARD_TYPE_H
// ClipboardItem representation types, as the donor's
// ClipboardItem::ParseMimeType (dom/events/ClipboardItem.cpp) decides them:
// a mandatory type (Clipboard::MandatoryDataTypes) is kept verbatim, anything
// else must be a WHATWG MIME type and is kept as its serialization. Custom
// "web " formats need dom.clipboard.customFormatSupport.enabled, which the
// donor build ships off, so they are invalid. No engine dependency.
#include <string>
#include <utility>
#include <vector>

namespace ruyijs::domain::clipboard {
struct ClipboardType {
  bool valid = false;
  // supports(): only a mandatory type is a supported representation.
  bool supported = false;
  std::u16string type;
};

namespace detail {
inline bool HttpWhitespace(char16_t c) { return c == u'\t' || c == u'\n' || c == u'\r' || c == u' '; }
inline bool TokenCodePoint(char16_t c) {
  if ((c >= u'0' && c <= u'9') || (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z')) return true;
  for (char16_t symbol : std::u16string(u"!#$%&'*+-.^_`|~")) if (c == symbol) return true;
  return false;
}
inline bool Token(const std::u16string& text) {
  if (text.empty()) return false;
  for (char16_t c : text) if (!TokenCodePoint(c)) return false;
  return true;
}
inline bool QuotedStringToken(const std::u16string& text) {
  for (char16_t c : text)
    if (!(c == u'\t' || (c >= 0x20 && c <= 0x7e) || (c >= 0x80 && c <= 0xff))) return false;
  return true;
}
inline std::u16string Lower(std::u16string text) {
  for (char16_t& c : text) if (c >= u'A' && c <= u'Z') c = char16_t(c + (u'a' - u'A'));
  return text;
}
inline std::u16string TrimEnd(std::u16string text) {
  while (!text.empty() && HttpWhitespace(text.back())) text.pop_back();
  return text;
}
}  // namespace detail

// WHATWG "parse a MIME type" followed by "serialize a MIME type"; returns false
// on failure.
inline bool SerializeMimeType(const std::u16string& input, std::u16string* out) {
  using namespace detail;
  size_t begin = 0, end = input.size();
  while (begin < end && HttpWhitespace(input[begin])) ++begin;
  while (end > begin && HttpWhitespace(input[end - 1])) --end;
  const std::u16string text = input.substr(begin, end - begin);
  size_t position = text.find(u'/');
  if (position == text.npos) return false;
  const std::u16string type = text.substr(0, position);
  if (!Token(type)) return false;
  ++position;
  size_t semicolon = text.find(u';', position);
  const std::u16string subtype = TrimEnd(text.substr(position, semicolon == text.npos
                                                                   ? text.npos : semicolon - position));
  if (!Token(subtype)) return false;
  std::vector<std::pair<std::u16string, std::u16string>> parameters;
  position = semicolon;
  while (position != text.npos && position < text.size()) {
    ++position;
    while (position < text.size() && HttpWhitespace(text[position])) ++position;
    size_t stop = text.find_first_of(u";=", position);
    std::u16string name = Lower(text.substr(position, stop == text.npos ? text.npos : stop - position));
    if (stop == text.npos) break;
    position = stop;
    if (text[position] == u';') continue;
    ++position;
    if (position >= text.size()) break;
    std::u16string value;
    if (text[position] == u'"') {
      ++position;
      while (position < text.size()) {
        const char16_t c = text[position];
        if (c == u'"') { ++position; break; }
        if (c == u'\\') {
          if (++position >= text.size()) { value += u'\\'; break; }
          value += text[position++];
          continue;
        }
        value += c;
        ++position;
      }
      position = text.find(u';', position);
    } else {
      stop = text.find(u';', position);
      value = TrimEnd(text.substr(position, stop == text.npos ? text.npos : stop - position));
      position = stop;
      if (value.empty()) continue;
    }
    bool seen = false;
    for (const auto& parameter : parameters) seen = seen || parameter.first == name;
    if (Token(name) && QuotedStringToken(value) && !seen) parameters.emplace_back(name, value);
  }
  *out = Lower(type) + u'/' + Lower(subtype);
  for (const auto& [name, value] : parameters) {
    *out += u';' + name + u'=';
    if (Token(value)) { *out += value; continue; }
    *out += u'"';
    for (char16_t c : value) {
      if (c == u'"' || c == u'\\') *out += u'\\';
      *out += c;
    }
    *out += u'"';
  }
  return true;
}

inline ClipboardType ParseClipboardType(const std::u16string& input) {
  ClipboardType result;
  for (const char16_t* mandatory : {u"text/html", u"text/plain", u"image/png"}) {
    if (input == mandatory) {
      result.valid = result.supported = true;
      result.type = input;
      return result;
    }
  }
  if (input.rfind(u"web ", 0) == 0) return result;
  result.valid = SerializeMimeType(input, &result.type);
  return result;
}
}  // namespace ruyijs::domain::clipboard
#endif
