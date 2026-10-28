#ifndef RUYIJS_WEB_GRAPHICS_SANITIZE_INTERNAL_H
#define RUYIJS_WEB_GRAPHICS_SANITIZE_INTERNAL_H
#include <climits>
#include <string>

namespace ruyijs::web_graphics::sanitize_detail {
inline bool Contains(const std::string& text, const std::string& part) {
  return text.find(part) != std::string::npos;
}
inline bool Contains(const std::string& text, const char* part) {
  return text.find(part) != std::string::npos;
}
// The donor calls std::stoul on regex captures. This port parses the same digit
// runs without throwing, because every product translation unit is built with
// exceptions disabled unless product_sources.py lists it.
inline bool ParseDigits(const std::string& text, unsigned long base,
                        unsigned long* out) {
  if (text.empty()) return false;
  unsigned long value = 0;
  for (char character : text) {
    unsigned long digit;
    if (character >= '0' && character <= '9') {
      digit = (unsigned long)(character - '0');
    } else if (base == 16 && character >= 'A' && character <= 'F') {
      digit = (unsigned long)(character - 'A' + 10);
    } else if (base == 16 && character >= 'a' && character <= 'f') {
      digit = (unsigned long)(character - 'a' + 10);
    } else {
      return false;
    }
    if (value > (ULONG_MAX - digit) / base) return false;
    value = value * base + digit;
  }
  *out = value;
  return true;
}
}  // namespace ruyijs::web_graphics::sanitize_detail
#endif
