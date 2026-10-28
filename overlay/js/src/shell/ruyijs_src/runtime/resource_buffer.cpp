// Protocol bytes only: no JSAPI calls or persistent resource state.
#include "runtime/resource_buffer.h"
#include <new>

namespace ruyijs::resource_wire {
using resource_network::ErrorCode;
using resource_network::Limits;
static Status OOM() { return {ErrorCode::OutOfMemory, "Resource protocol allocation failed"}; }
Status Allocate(std::string* output, size_t bytes) noexcept {
  try { output->resize(bytes); return {}; } catch (...) { return OOM(); }
}
Status Append(resource_network::HeaderPairs* output, std::string name, std::string value) noexcept {
  try { output->emplace_back(std::move(name), std::move(value)); return {}; }
  catch (...) { return OOM(); }
}
static int Digit(unsigned char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  return c == '+' ? 62 : c == '/' ? 63 : -1;
}
Status DecodeBase64(std::string_view input, std::string* output) noexcept {
  const Status bad{ErrorCode::Type, "Resource body must use canonical base64"};
  if (input.size() % 4) return bad;
  size_t padding = !input.empty() && input.back() == '=' ? 1 : 0;
  if (padding && input[input.size() - 2] == '=') ++padding;
  const size_t length = input.size() / 4 * 3 - padding;
  if (length > Limits::body_bytes) return {ErrorCode::Range, "Resource body exceeds 8 MiB"};
  for (size_t i = 0; i < input.size() - padding; ++i)
    if (Digit(static_cast<unsigned char>(input[i])) < 0) return bad;
  if (padding && (Digit(input[input.size() - padding - 1]) & (padding == 2 ? 15 : 3))) return bad;
  try {
    std::string decoded(length, '\0');
    size_t offset = 0;
    for (size_t i = 0; i < input.size(); i += 4) {
      unsigned bits = unsigned(Digit(input[i])) << 18 | unsigned(Digit(input[i + 1])) << 12;
      if (input[i + 2] != '=') bits |= unsigned(Digit(input[i + 2])) << 6;
      if (input[i + 3] != '=') bits |= unsigned(Digit(input[i + 3]));
      for (int shift : {16, 8, 0}) if (offset < length) decoded[offset++] = char(bits >> shift);
    }
    output->swap(decoded);
    return {};
  } catch (...) { return OOM(); }
}
Status EncodeBase64(std::string_view input, std::string* output) noexcept {
  static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  if (input.size() > Limits::body_bytes) return {ErrorCode::Range, "Resource body exceeds 8 MiB"};
  try {
    std::string encoded((input.size() + 2) / 3 * 4, '=');
    size_t offset = 0;
    for (size_t i = 0; i < input.size(); i += 3) {
      unsigned bits = unsigned(static_cast<unsigned char>(input[i])) << 16;
      if (i + 1 < input.size()) bits |= unsigned(static_cast<unsigned char>(input[i + 1])) << 8;
      if (i + 2 < input.size()) bits |= static_cast<unsigned char>(input[i + 2]);
      encoded[offset++] = alphabet[bits >> 18];
      encoded[offset++] = alphabet[(bits >> 12) & 63];
      if (i + 1 < input.size()) encoded[offset] = alphabet[(bits >> 6) & 63];
      ++offset;
      if (i + 2 < input.size()) encoded[offset] = alphabet[bits & 63];
      ++offset;
    }
    output->swap(encoded);
    return {};
  } catch (...) { return OOM(); }
}
bool ValidUTF8(std::string_view text) noexcept {
  for (size_t i = 0; i < text.size();) {
    unsigned first = static_cast<unsigned char>(text[i++]);
    if (first < 0x80) continue;
    unsigned count, code, minimum;
    if (first >= 0xc2 && first <= 0xdf) { count = 1; code = first & 31; minimum = 0x80; }
    else if (first >= 0xe0 && first <= 0xef) { count = 2; code = first & 15; minimum = 0x800; }
    else if (first >= 0xf0 && first <= 0xf4) { count = 3; code = first & 7; minimum = 0x10000; }
    else return false;
    if (count > text.size() - i) return false;
    while (count--) {
      unsigned next = static_cast<unsigned char>(text[i++]);
      if ((next & 0xc0) != 0x80) return false;
      code = (code << 6) | (next & 63);
    }
    if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return false;
  }
  return true;
}
}
