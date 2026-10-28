#ifndef RUYIJS_DOMAIN_CREDENTIALS_BASE64URL_H
#define RUYIJS_DOMAIN_CREDENTIALS_BASE64URL_H
// Base64URLString decoding as the donor's Base64URLDecode with
// Base64URLDecodePaddingPolicy::Ignore (xpcom/io/Base64.cpp): trailing "="
// is stripped only when the length is a multiple of four, a final quantum of
// one character fails, and leftover low bits are ignored. No engine dependency.
#include <cstdint>
#include <string>

namespace ruyijs::domain::credentials {
inline bool Base64UrlValue(char16_t c, uint8_t* value) {
  if (c >= u'A' && c <= u'Z') { *value = uint8_t(c - u'A'); return true; }
  if (c >= u'a' && c <= u'z') { *value = uint8_t(c - u'a' + 26); return true; }
  if (c >= u'0' && c <= u'9') { *value = uint8_t(c - u'0' + 52); return true; }
  if (c == u'-') { *value = 62; return true; }
  if (c == u'_') { *value = 63; return true; }
  return false;
}

inline bool DecodeBase64Url(const std::u16string& input, std::string* bytes) {
  bytes->clear();
  size_t length = input.size();
  if (length && length % 4 == 0 && input[length - 1] == u'=')
    length -= input[length - 2] == u'=' ? 2 : 1;
  size_t index = 0;
  uint8_t v[4];
  for (; length - index >= 4; index += 4) {
    for (int i = 0; i < 4; ++i) if (!Base64UrlValue(input[index + i], &v[i])) return false;
    bytes->push_back(char(uint8_t(v[0] << 2 | v[1] >> 4)));
    bytes->push_back(char(uint8_t(v[1] << 4 | v[2] >> 2)));
    bytes->push_back(char(uint8_t(v[2] << 6 | v[3])));
  }
  const size_t rest = length - index;
  if (rest == 1) return false;
  for (size_t i = 0; i < rest; ++i) if (!Base64UrlValue(input[index + i], &v[i])) return false;
  if (rest >= 2) bytes->push_back(char(uint8_t(v[0] << 2 | v[1] >> 4)));
  if (rest == 3) bytes->push_back(char(uint8_t(v[1] << 4 | v[2] >> 2)));
  return true;
}
}  // namespace ruyijs::domain::credentials
#endif
