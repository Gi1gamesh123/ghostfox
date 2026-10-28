// Existing CSS.escape serializer, shared with CSSOM rule names.
#include "domain/styling/css_identifier.h"
#include <cstdio>
namespace ruyijs::domain::styling {
namespace {
bool IsDigit(char c) { return c >= '0' && c <= '9'; }
bool IsAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

}
// The CSSOM "serialize an identifier" algorithm, over the UTF-8 bytes.
std::string CssEscapeIdentifier(const std::string& input) {
  std::string out;
  for (size_t index = 0; index < input.size(); ++index) {
    unsigned char code = static_cast<unsigned char>(input[index]);
    if (code == 0) {
      out += "\xEF\xBF\xBD";
      continue;
    }
    bool first = index == 0;
    bool second_digit = index == 1 && input[0] == '-' && IsDigit(char(code));
    if ((first && IsDigit(char(code))) || second_digit) {
      char buffer[8];
      std::snprintf(buffer, sizeof(buffer), "\\%x ", code);
      out += buffer;
      continue;
    }
    if (code < 0x20 || code == 0x7F) {
      char buffer[8];
      std::snprintf(buffer, sizeof(buffer), "\\%x ", code);
      out += buffer;
      continue;
    }
    if (code >= 0x80 || code == '-' || code == '_' || IsAlpha(char(code)) || IsDigit(char(code))) {
      out.push_back(char(code));
      continue;
    }
    out.push_back('\\');
    out.push_back(char(code));
  }
  return out;
}

}
