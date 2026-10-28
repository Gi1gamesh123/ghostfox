#ifndef RUYIJS_DOMAIN_WEB_MEDIA_MIME_H
#define RUYIJS_DOMAIN_WEB_MEDIA_MIME_H
#include <string>
#include <string_view>
#include <vector>
namespace ruyijs::domain::web_media {
inline std::string Trim(std::string_view value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == value.npos) return {};
  return std::string(value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1));
}
inline std::string Lower(std::string value) {
  for (char& c : value) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
  return value;
}
struct MediaType {
  std::string mime;
  std::vector<std::string> codecs;
  size_t parameters = 0;
  bool valid = false, has_codecs = false;
};
inline bool Token(std::string_view text) {
  if (text.empty()) return false;
  for (unsigned char c : text) {
    if (c <= 32 || c >= 127 || std::string_view("()<>@,;:\\\"/[]?={}").find(c) != text.npos) return false;
  }
  return true;
}
inline MediaType ParseMediaType(std::string_view raw) {
  MediaType out;
  const std::string text = Trim(raw);
  size_t pos = text.find(';');
  out.mime = Lower(Trim(std::string_view(text).substr(0, pos)));
  const auto slash = out.mime.find('/');
  if (slash == out.mime.npos || !Token(out.mime.substr(0, slash)) || !Token(out.mime.substr(slash + 1))) return out;
  out.valid = true;
  std::vector<std::string> names;
  while (pos != text.npos && pos < text.size()) {
    ++pos;
    while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t')) ++pos;
    size_t end = text.find_first_of("=;", pos);
    if (end == text.npos) break;
    if (text[end] == ';') { pos = end; continue; }
    std::string name = Lower(Trim(std::string_view(text).substr(pos, end - pos)));
    pos = end + 1;
    std::string value;
    if (pos < text.size() && text[pos] == '"') {
      ++pos;
      while (pos < text.size() && text[pos] != '"') {
        if (text[pos] == '\\' && pos + 1 < text.size()) ++pos;
        value += text[pos++];
      }
      pos = text.find(';', pos);
    } else {
      end = text.find(';', pos);
      value = Trim(std::string_view(text).substr(pos, end == text.npos ? end : end - pos));
      pos = end;
    }
    if (!Token(name)) continue;
    bool duplicate = false;
    for (const auto& seen : names) duplicate |= seen == name;
    if (duplicate) continue;
    names.push_back(name); ++out.parameters;
    if (name != "codecs") continue;
    out.has_codecs = true;
    if (value.empty()) continue;
    size_t start = 0;
    do {
      end = value.find(',', start);
      out.codecs.push_back(Trim(std::string_view(value).substr(start, end == value.npos ? end : end - start)));
      if (end == value.npos) break;
      start = end + 1;
    } while (start <= value.size());
  }
  return out;
}
}
#endif
