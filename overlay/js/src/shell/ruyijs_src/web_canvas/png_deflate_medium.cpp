// The level-3 parser of the deflater: zlib-rs' "medium" algorithm at the level
// where it looks no further than the current match, its symbol buffer, and the
// one-shot entry point that wraps the blocks in a zlib stream.
#include "web_canvas/png_deflate.h"

namespace ruyijs::web_canvas::detail {
namespace {
// zlib's adler32 over the whole uncompressed input.
uint32_t Adler32(const uint8_t* data, size_t length) {
  uint32_t first = 1;
  uint32_t second = 0;
  for (size_t index = 0; index < length; ++index) {
    first = (first + data[index]) % 65521u;
    second = (second + first) % 65521u;
  }
  return (second << 16) | first;
}
}  // namespace

bool Deflater::TallyLiteral(uint8_t literal) {
  sym[symbols * 3 + 2] = literal;  // the two distance bytes stay zero
  symbols += 1;
  ltree[literal].freq += 1;
  return ShouldFlush();
}

// `length` is the zero-based length code index, that is match length - kMinMatch.
bool Deflater::TallyMatch(size_t dist, size_t length) {
  sym[symbols * 3] = static_cast<uint8_t>(dist & 0xff);
  sym[symbols * 3 + 1] = static_cast<uint8_t>((dist >> 8) & 0xff);
  sym[symbols * 3 + 2] = static_cast<uint8_t>(length);
  symbols += 1;
  ltree[Tables().length_code[length] + 256 + 1].freq += 1;
  dtree[DistCode(dist - 1)].freq += 1;
  return ShouldFlush();
}

// Register the strings a decided match covers, the way zlib-rs' insert_match
// does: every interior position up to a bounded match length.
void Deflater::InsertMatch(size_t orgstart, size_t length) {
  if (lookahead <= length + kWantMinMatch) return;
  if (length < kWantMinMatch) {
    if (length > 1 && orgstart + length > orgstart) InsertStrings(orgstart + 1, length - 1);
    return;
  }
  if (length <= 16 * size_t(max_lazy) && lookahead >= kWantMinMatch) {
    InsertStrings(orgstart + 1, length - 1);
    return;
  }
  const size_t next = orgstart + length;
  if (next >= kMinMatch - 2) QuickInsert(next + 2 - kMinMatch);
}

void Deflater::Medium() {
  for (;;) {
    if (lookahead < kMinLookahead) {
      FillWindow();
      if (lookahead == 0) break;
    }
    size_t hash_head = 0;
    if (lookahead >= kWantMinMatch) hash_head = QuickInsert(strstart);
    size_t length = 1;
    size_t start = 0;
    if (hash_head != 0 && strstart > hash_head && strstart - hash_head <= max_dist) {
      size_t found = match_start;
      length = LongestMatch(hash_head, &found);
      match_start = found;
      start = found;
      if (length < kWantMinMatch || start >= strstart) length = 1;
    }
    InsertMatch(strstart, length);
    bool flush = false;
    if (length < kWantMinMatch) {
      flush = TallyLiteral(window[strstart]);
    } else {
      flush = TallyMatch(strstart - start, length - kMinMatch);
    }
    lookahead -= length;
    strstart += length;
    if (flush) FlushBlock(false);
  }
  insert = strstart < kMinMatch - 1 ? strstart : kMinMatch - 1;
  FlushBlock(true);
}

Deflater::Deflater(const uint8_t* data, size_t size, int window_bits) {
  source = data;
  source_size = size;
  in = data;
  avail_in = size;
  w_bits = window_bits < 9 ? 9 : window_bits;  // zlib turns an 8-bit window into 9
  w_size = size_t(1) << w_bits;
  window_size = 2 * w_size;
  max_dist = w_size - kMinLookahead;
  window.assign(window_size + kMaxMatch, 0);
  head.assign(kHashSize, 0);
  prev.assign(w_size, 0);
  sym.assign(kLitBufSize * 3, 0);
  InitBlock();
}

std::string Deflater::Run() {
  // The zlib header: deflate with the nominal window, level flags for level 3,
  // no preset dictionary. libpng rewrites the window bits afterwards.
  unsigned header = (8u + ((unsigned(w_bits) - 8u) << 4)) << 8;
  header |= 1u << 6;
  header += 31 - (header % 31);
  bits.out.push_back(static_cast<char>((header >> 8) & 0xff));
  bits.out.push_back(static_cast<char>(header & 0xff));
  Medium();
  const uint32_t adler = Adler32(source, source_size);
  for (int shift = 24; shift >= 0; shift -= 8)
    bits.out.push_back(static_cast<char>((adler >> shift) & 0xff));
  return bits.out;
}
}  // namespace ruyijs::web_canvas::detail
