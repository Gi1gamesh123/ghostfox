#include "transport/worker_json_scan.h"
#include <array>

namespace ruyijs::transport::worker {
namespace {
int Hex(char c) noexcept {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
bool Unit(std::string_view text, size_t* at, unsigned* result) noexcept {
  if (text.size() - *at < 4) return false;
  unsigned value = 0;
  for (unsigned i = 0; i < 4; ++i) {
    int digit = Hex(text[(*at)++]);
    if (digit < 0) return false;
    value = (value << 4) | static_cast<unsigned>(digit);
  }
  *result = value;
  return true;
}
Status String(std::string_view text, size_t* at) noexcept {
  while (*at < text.size()) {
    unsigned char c = static_cast<unsigned char>(text[(*at)++]);
    if (c == '"') return {};
    if (c < 0x20) return Failure(Error::Escape, *at - 1);
    if (c != '\\') continue;
    if (*at == text.size()) return Failure(Error::Escape, *at);
    c = static_cast<unsigned char>(text[(*at)++]);
    if (c == '"' || c == '\\' || c == '/' || c == 'b' || c == 'f' || c == 'n' || c == 'r' || c == 't') continue;
    if (c != 'u') return Failure(Error::Escape, *at - 1);
    unsigned first;
    if (!Unit(text, at, &first)) return Failure(Error::Escape, *at);
    if (first >= 0xdc00 && first <= 0xdfff) return Failure(Error::Escape, *at - 4);
    if (first < 0xd800 || first > 0xdbff) continue;
    if (text.size() - *at < 6 || text[*at] != '\\' || text[*at + 1] != 'u')
      return Failure(Error::Escape, *at);
    *at += 2;
    unsigned second;
    if (!Unit(text, at, &second) || second < 0xdc00 || second > 0xdfff)
      return Failure(Error::Escape, *at);
  }
  return Failure(Error::Escape, *at);
}
bool Digit(char c) noexcept { return c >= '0' && c <= '9'; }
}
Status ScanJsonQuota(std::string_view text) noexcept {
  if (text.empty() || text.size() > kControlLimit) return Failure(Error::Length);
  Status utf8 = ValidateUtf8(text);
  if (!utf8) return utf8;
  if (text.size() >= 3 && text.substr(0, 3) == "\xef\xbb\xbf") return Failure(Error::Utf8);
  std::array<char, kJsonDepthLimit> containers{};
  size_t depth = 0, nodes = 0;
  for (size_t i = 0; i < text.size();) {
    char c = text[i++];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',' || c == ':') continue;
    if (c == '}' || c == ']') {
      if (!depth || containers[--depth] != (c == '}' ? '{' : '[')) return Failure(Error::Json, i - 1);
      continue;
    }
    if (++nodes > kJsonNodeLimit) return Failure(Error::Quota, i - 1);
    if (c == '{' || c == '[') {
      if (depth == kJsonDepthLimit) return Failure(Error::Quota, i - 1);
      containers[depth++] = c;
    } else if (c == '"') {
      Status status = String(text, &i);
      if (!status) return status;
    } else if (Digit(c)) {
      size_t begin = i - 1;
      uint64_t number = static_cast<unsigned>(c - '0');
      while (i < text.size() && Digit(text[i])) {
        number = number * 10 + static_cast<unsigned>(text[i++] - '0');
        if (number > UINT32_MAX) return Failure(Error::Number, begin);
      }
      if (i - begin > 1 && c == '0') return Failure(Error::Number, begin);
      if (i < text.size() && (text[i] == '.' || text[i] == 'e' || text[i] == 'E'))
        return Failure(Error::Number, begin);
    } else {
      std::string_view token = c == 't' ? "true" : c == 'f' ? "false" : c == 'n' ? "null" : "";
      if (token.empty() || text.substr(i - 1, token.size()) != token) return Failure(Error::Json, i - 1);
      i += token.size() - 1;
    }
  }
  return depth ? Failure(Error::Json, text.size()) : Status{};
}
}
