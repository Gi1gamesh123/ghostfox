#include "transport/worker_json_scan.h"
namespace ruyijs::transport::worker {
Status ValidateUtf8(std::string_view text) noexcept {
  for (size_t i = 0; i < text.size();) {
    const size_t start = i;
    unsigned char first = static_cast<unsigned char>(text[i++]);
    if (first <= 0x7f) continue;
    unsigned count;
    uint32_t value, minimum;
    if (first >= 0xc2 && first <= 0xdf) { count = 1; value = first & 31; minimum = 0x80; }
    else if (first >= 0xe0 && first <= 0xef) { count = 2; value = first & 15; minimum = 0x800; }
    else if (first >= 0xf0 && first <= 0xf4) { count = 3; value = first & 7; minimum = 0x10000; }
    else return Failure(Error::Utf8, start);
    if (count > text.size() - i) return Failure(Error::Utf8, start);
    while (count--) {
      unsigned char c = static_cast<unsigned char>(text[i++]);
      if ((c & 0xc0) != 0x80) return Failure(Error::Utf8, i - 1);
      value = (value << 6) | (c & 63);
    }
    if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
      return Failure(Error::Utf8, start);
  }
  return {};
}
Status ValidateU64(std::string_view text, uint64_t* result) noexcept {
  if (!result || text.empty() || text.size() > 20 || (text.size() > 1 && text.front() == '0'))
    return Failure(Error::Number);
  uint64_t value = 0;
  for (size_t i = 0; i < text.size(); ++i) {
    unsigned digit = static_cast<unsigned char>(text[i]) - static_cast<unsigned>('0');
    if (digit > 9 || value > (UINT64_MAX - digit) / 10) return Failure(Error::Number, i);
    value = value * 10 + digit;
  }
  *result = value;
  return {};
}
}
