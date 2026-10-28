// The LZ77 window of the deflater: filling and sliding it, the hash chains, and
// the longest-match search at the level-3 chain and nice-match limits.
#include "web_canvas/png_deflate.h"

#include <cstring>

namespace ruyijs::web_canvas::detail {
namespace {
// zlib-rs' standard hash: four bytes times the golden ratio, top bits only.
uint32_t HashValue(const uint8_t* bytes) {
  const uint32_t value = uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) |
                         (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
  return ((value * 2654435761u) >> 16) & uint32_t(kHashSize - 1);
}

// The number of leading equal bytes of two windows, at most limit bytes.
size_t CommonPrefix(const uint8_t* first, const uint8_t* second, size_t limit) {
  size_t length = 0;
  while (length < limit && first[length] == second[length]) ++length;
  return length;
}

uint64_t Word(const uint8_t* bytes) {
  uint64_t value = 0;
  std::memcpy(&value, bytes, sizeof value);
  return value;
}

// The byte pair compared to reject a candidate without a full comparison.
size_t EdgeOffset(size_t length) {
  size_t offset = length - 1;
  if (length >= 4) {
    offset -= 2;
    if (length >= 8) offset -= 4;
  }
  return offset;
}
}  // namespace

uint16_t Deflater::QuickInsert(size_t string) {
  const uint32_t hash = HashValue(&window[string]);
  const uint16_t head_value = head[hash];
  if (head_value != static_cast<uint16_t>(string)) {
    prev[string & (w_size - 1)] = head_value;
    head[hash] = static_cast<uint16_t>(string);
  }
  return head_value;
}

void Deflater::InsertStrings(size_t string, size_t count) {
  for (size_t index = 0; index < count; ++index) {
    const size_t position = string + index;
    const uint32_t hash = HashValue(&window[position]);
    const uint16_t head_value = head[hash];
    if (head_value != static_cast<uint16_t>(position)) {
      prev[position & (w_size - 1)] = head_value;
      head[hash] = static_cast<uint16_t>(position);
    }
  }
}

// zlib-rs' longest_match, as the medium parser calls it: walk the hash chain
// from cur_match, keep the best length, stop at the chain limit or nice_match.
// A candidate whose fast check fails moves on in the chain; one that reaches a
// full comparison without improving ends the search at these low levels.
size_t Deflater::LongestMatch(size_t cur_match, size_t* out_match_start) {
  size_t best_len = prev_length > 0 ? prev_length : kMinMatch - 1;
  size_t chain = max_chain;
  if (best_len >= good_match) chain >>= 2;
  const size_t limit = strstart > max_dist ? strstart - max_dist : 0;
  size_t offset = EdgeOffset(best_len);
  while (true) {
    if (cur_match >= strstart) break;
    size_t length = 0;
    bool compared = true;
    if (best_len < 8) {
      const uint64_t difference = Word(&window[strstart]) ^ Word(&window[cur_match]);
      if (difference != 0) {
        const size_t equal = CommonPrefix(&window[strstart], &window[cur_match], 8);
        compared = false;
        if (equal > best_len) {
          length = equal;
          compared = true;
        }
      }
    } else {
      compared = Word(&window[strstart]) == Word(&window[cur_match]) &&
                 Word(&window[strstart + offset]) == Word(&window[cur_match + offset]);
    }
    if (compared) {
      if (length == 0)
        length = 2 + CommonPrefix(&window[cur_match + 2], &window[strstart + 2], kMaxMatch - 2);
      if (length > best_len) {
        *out_match_start = cur_match;
        if (length >= lookahead) return lookahead;
        best_len = length;
        if (best_len >= nice_match) return best_len;
        offset = EdgeOffset(best_len);
      } else {
        break;
      }
    }
    chain -= 1;
    if (chain == 0) break;
    cur_match = prev[cur_match & (w_size - 1)];
    if (cur_match <= limit) break;
  }
  return best_len;
}

void Deflater::SlideHash() {
  for (size_t index = 0; index < head.size(); ++index)
    head[index] = head[index] > w_size ? uint16_t(head[index] - w_size) : 0;
  for (size_t index = 0; index < prev.size(); ++index)
    prev[index] = prev[index] > w_size ? uint16_t(prev[index] - w_size) : 0;
}

void Deflater::FillWindow() {
  for (;;) {
    size_t more = window_size - lookahead - strstart;
    if (strstart >= w_size + max_dist) {
      std::memcpy(&window[0], &window[w_size], w_size);
      if (match_start >= w_size) {
        match_start -= w_size;
      } else {
        match_start = 0;
        prev_length = 0;
      }
      strstart -= w_size;
      block_start -= int64_t(w_size);
      if (insert > strstart) insert = strstart;
      SlideHash();
      more += w_size;
    }
    if (avail_in == 0) break;
    const size_t take = avail_in < more ? avail_in : more;
    std::memcpy(&window[strstart + lookahead], in, take);
    in += take;
    avail_in -= take;
    lookahead += take;
    if (lookahead + insert >= kMinMatch) {
      const size_t string = strstart - insert;
      if (string >= 1) QuickInsert(string + 2 - kMinMatch);
      size_t count = insert;
      if (lookahead == 1) count -= 1;
      if (count > 0) {
        InsertStrings(string, count);
        insert -= count;
      }
    }
    if (!(lookahead < kMinLookahead && avail_in != 0)) break;
  }
}
}  // namespace ruyijs::web_canvas::detail
