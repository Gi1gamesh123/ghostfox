// The tables deflate builds once from RFC 1951: the length and distance code
// maps plus the two static Huffman trees. Building them here keeps the code
// tables in the same place as the algorithm that reads them.
#include "web_canvas/png_deflate.h"

namespace ruyijs::web_canvas::detail {
namespace {
// zlib's gen_codes(): canonical codes from bit lengths, bit-reversed because
// deflate sends the most significant code bit last.
void GenCodes(const uint16_t* lengths, size_t count, uint16_t* codes, size_t max_bits) {
  uint16_t next_code[kMaxBits + 1] = {};
  uint16_t count_per_length[kMaxBits + 1] = {};
  unsigned code = 0;
  for (size_t index = 0; index < count; ++index)
    if (lengths[index] != 0) count_per_length[lengths[index]] += 1;
  for (size_t bits = 1; bits <= max_bits; ++bits) {
    code = (code + count_per_length[bits - 1]) << 1;
    next_code[bits] = static_cast<uint16_t>(code);
  }
  for (size_t index = 0; index < count; ++index) {
    const unsigned length = lengths[index];
    if (length == 0) continue;
    unsigned value = next_code[length]++;
    unsigned reversed = 0;
    for (unsigned bit = 0; bit < length; ++bit) reversed |= ((value >> bit) & 1u) << (length - 1 - bit);
    codes[index] = static_cast<uint16_t>(reversed);
  }
}
}  // namespace

DeflateTables::DeflateTables() {
  // Lengths 3..258 map onto length codes 0..28; code 28 covers the single
  // length 258 that has no extra bits, so the table stops one code early.
  size_t length = 0;
  for (size_t code = 0; code + 1 < 29; ++code)
    for (size_t repeat = 0; repeat < (size_t(1) << kExtraLBits[code]); ++repeat)
      length_code[length++] = static_cast<uint8_t>(code);
  // Length 258 has two encodings; code 28 needs no extra bits, so it wins.
  length_code[length - 1] = 28;
  // Distances 1..256 are sent directly, longer distances through the offset
  // half of the table indexed by distance >> 7.
  size_t dist = 0;
  size_t code = 0;
  for (; code < 16; ++code)
    for (size_t repeat = 0; repeat < (size_t(1) << kExtraDBits[code]); ++repeat)
      dist_code[dist++] = static_cast<uint8_t>(code);
  dist >>= 7;
  for (; code < 30; ++code)
    for (size_t repeat = 0; repeat < (size_t(1) << (kExtraDBits[code] - 7)); ++repeat)
      dist_code[256 + dist++] = static_cast<uint8_t>(code);
  // The fixed literal/length and distance trees of RFC 1951 section 3.2.6.
  uint16_t llen[288] = {};
  for (size_t symbol = 0; symbol < 144; ++symbol) llen[symbol] = 8;
  for (size_t symbol = 144; symbol < 256; ++symbol) llen[symbol] = 9;
  for (size_t symbol = 256; symbol < 280; ++symbol) llen[symbol] = 7;
  for (size_t symbol = 280; symbol < 288; ++symbol) llen[symbol] = 8;
  GenCodes(llen, 288, static_lcode, kMaxBits);
  for (size_t symbol = 0; symbol < 288; ++symbol) static_llen[symbol] = llen[symbol];
  uint16_t dlen[30];
  for (size_t symbol = 0; symbol < 30; ++symbol) {
    dlen[symbol] = 5;
    static_dlen[symbol] = 5;
  }
  GenCodes(dlen, 30, static_dcode, kMaxBits);
}

const DeflateTables& Tables() {
  static const DeflateTables tables;
  return tables;
}

// The bit-level writer. Emitting a byte as soon as eight bits accumulate gives
// the same stream as zlib's wider buffer, which only ever flushes whole bytes.
void BitWriter::Send(uint64_t value, unsigned length) {
  buffer |= value << used;
  used += length;
  while (used >= 8) {
    out.push_back(static_cast<char>(buffer & 0xff));
    buffer >>= 8;
    used -= 8;
  }
}

void BitWriter::Align() {
  if (used == 0) return;
  out.push_back(static_cast<char>(buffer & 0xff));
  buffer = 0;
  used = 0;
}
}  // namespace ruyijs::web_canvas::detail
