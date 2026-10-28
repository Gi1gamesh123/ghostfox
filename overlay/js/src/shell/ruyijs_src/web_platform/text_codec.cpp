// Original UTF-8 encoding algorithms. Decoding and label resolution live in
// text_decode.cpp and use the already-linked Firefox encoding_rs library.
#include "web_platform/text_codec.h"

namespace ruyijs::web_platform {
namespace {
// Decodes one UTF-16 code point, folding an unpaired surrogate to U+FFFD the way
// the encoder must when it cannot form a valid scalar value.
uint32_t CodePoint(std::u16string_view units, size_t index, size_t* consumed) {
  const char16_t unit = units[index];
  if (unit >= 0xD800 && unit <= 0xDBFF && index + 1 < units.size() &&
      units[index + 1] >= 0xDC00 && units[index + 1] <= 0xDFFF) {
    *consumed = 2;
    return 0x10000 + ((uint32_t(unit) - 0xD800) << 10) +
           (uint32_t(units[index + 1]) - 0xDC00);
  }
  *consumed = 1;
  if (unit >= 0xD800 && unit <= 0xDFFF) return 0xFFFD;
  return uint32_t(unit);
}

size_t ByteCount(uint32_t point) {
  if (point < 0x80) return 1;
  if (point < 0x800) return 2;
  if (point < 0x10000) return 3;
  return 4;
}

size_t Write(uint32_t point, uint8_t* out) {
  if (point < 0x80) {
    out[0] = uint8_t(point);
    return 1;
  }
  if (point < 0x800) {
    out[0] = uint8_t(0xC0 | (point >> 6));
    out[1] = uint8_t(0x80 | (point & 0x3F));
    return 2;
  }
  if (point < 0x10000) {
    out[0] = uint8_t(0xE0 | (point >> 12));
    out[1] = uint8_t(0x80 | ((point >> 6) & 0x3F));
    out[2] = uint8_t(0x80 | (point & 0x3F));
    return 3;
  }
  out[0] = uint8_t(0xF0 | (point >> 18));
  out[1] = uint8_t(0x80 | ((point >> 12) & 0x3F));
  out[2] = uint8_t(0x80 | ((point >> 6) & 0x3F));
  out[3] = uint8_t(0x80 | (point & 0x3F));
  return 4;
}
}  // namespace

bool AppendUtf8(std::u16string_view units, std::string* out) {
  for (size_t index = 0; index < units.size();) {
    size_t consumed = 0;
    const uint32_t point = CodePoint(units, index, &consumed);
    const size_t bytes = ByteCount(point);
    if (out->size() + bytes > kTextByteLimit) return false;
    uint8_t raw[4];
    Write(point, raw);
    out->append(reinterpret_cast<const char*>(raw), bytes);
    index += consumed;
  }
  return true;
}

void EncodeUtf8Into(std::u16string_view units, uint8_t* out, size_t capacity,
                    size_t* read, size_t* written) {
  size_t index = 0, used = 0;
  while (index < units.size()) {
    size_t consumed = 0;
    const uint32_t point = CodePoint(units, index, &consumed);
    const size_t bytes = ByteCount(point);
    if (used + bytes > capacity) break;
    used += Write(point, out + used);
    index += consumed;
  }
  *read = index;
  *written = used;
}
}  // namespace ruyijs::web_platform
