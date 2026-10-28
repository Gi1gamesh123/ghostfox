// Huffman construction for the deflater: zlib's optimal bit length assignment
// with its overflow repair, the bit-length tree scanner and the canonical code
// generation. The arithmetic is zlib's, not an equivalent one: the encoder must
// pick the same codes as Firefox's zlib-rs.
#include "web_canvas/png_deflate.h"

namespace ruyijs::web_canvas::detail {
namespace {
uint16_t Reverse(unsigned value, unsigned length) {
  unsigned reversed = 0;
  for (unsigned bit = 0; bit < length; ++bit) reversed |= ((value >> bit) & 1u) << (length - 1 - bit);
  return uint16_t(reversed);
}

// zlib's gen_bitlen: assign the optimal lengths, then repair code lengths that
// overflowed the tree's maximum.
void GenBitLen(HuffmanHeap& heap, TreeNode* tree, size_t max_code, const uint16_t* stree_len,
               const uint8_t* extra, size_t extra_base, size_t max_length,
               int64_t* opt_len, int64_t* static_len, uint16_t* bl_count) {
  for (size_t index = 0; index <= kMaxBits; ++index) bl_count[index] = 0;
  tree[heap.node[heap.max]].len = 0;
  int32_t overflow = 0;
  for (size_t h = heap.max + 1; h < kHeapSize; ++h) {
    const size_t n = heap.node[h];
    size_t bits = size_t(tree[tree[n].dad].len) + 1;
    if (bits > max_length) {
      bits = max_length;
      overflow += 1;
    }
    tree[n].len = uint16_t(bits);
    if (n > max_code) continue;
    bl_count[bits] += 1;
    size_t extra_bits = 0;
    if (n >= extra_base) extra_bits = extra[n - extra_base];
    const size_t freq = tree[n].freq;
    *opt_len += int64_t(freq * (bits + extra_bits));
    if (stree_len) *static_len += int64_t(freq * (size_t(stree_len[n]) + extra_bits));
  }
  if (overflow == 0) return;
  do {
    int bits = int(max_length) - 1;
    while (bl_count[bits] == 0) bits -= 1;
    bl_count[bits] -= 1;
    bl_count[bits + 1] += 2;
    bl_count[max_length] -= 1;
    overflow -= 2;
  } while (overflow > 0);
  size_t h = kHeapSize;
  for (int bits = int(max_length); bits >= 1; --bits) {
    size_t count = bl_count[bits];
    while (count != 0) {
      h -= 1;
      const size_t m = heap.node[h];
      if (m > max_code) continue;
      if (size_t(tree[m].len) != size_t(bits)) {
        *opt_len += int64_t(bits) * tree[m].freq;
        *opt_len -= int64_t(tree[m].len) * tree[m].freq;
        tree[m].len = uint16_t(bits);
      }
      count -= 1;
    }
  }
}

void GenCodes(TreeNode* tree, size_t max_code, const uint16_t* bl_count) {
  uint16_t next_code[kMaxBits + 1] = {};
  unsigned code = 0;
  for (size_t bits = 1; bits <= kMaxBits; ++bits) {
    code = (code + bl_count[bits - 1]) << 1;
    next_code[bits] = uint16_t(code);
  }
  for (size_t n = 0; n <= max_code; ++n) {
    const unsigned length = tree[n].len;
    if (length == 0) continue;
    tree[n].code = Reverse(next_code[length]++, length);
  }
}

// zlib's scan_tree: count the bit lengths of one tree into the bit-length tree.
void ScanTree(TreeNode* bltree, TreeNode* tree, size_t max_code) {
  int64_t previous = -1;
  unsigned next = tree[0].len;
  size_t count = 0;
  size_t max_count = 7;
  size_t min_count = 4;
  if (next == 0) {
    max_count = 138;
    min_count = 3;
  }
  tree[max_code + 1].len = 0xffff;  // guard
  for (size_t n = 0; n <= max_code; ++n) {
    const unsigned current = next;
    next = tree[n + 1].len;
    count += 1;
    if (count < max_count && int64_t(current) == int64_t(next)) continue;
    if (count < min_count) {
      bltree[current].freq = uint16_t(bltree[current].freq + count);
    } else if (current != 0) {
      if (int64_t(current) != previous) bltree[current].freq += 1;
      bltree[kRep3To6].freq += 1;
    } else if (count <= 10) {
      bltree[kRepZ3To10].freq += 1;
    } else {
      bltree[kRepZ11To138].freq += 1;
    }
    count = 0;
    previous = int64_t(current);
    if (next == 0) {
      max_count = 138;
      min_count = 3;
    } else if (int64_t(current) == int64_t(next)) {
      max_count = 6;
      min_count = 3;
    } else {
      max_count = 7;
      min_count = 4;
    }
  }
}
}  // namespace

void Deflater::BuildTree(TreeNode* tree, size_t elements, const uint16_t* stree_len,
                         const uint8_t* extra, size_t extra_base, size_t max_length,
                         size_t* out_max_code) {
  HuffmanHeap heap;
  int64_t max_code = heap.Initialize(tree, elements);
  // At least two codes of non-zero frequency, so every tree has bit lengths.
  while (heap.len < 2) {
    heap.len += 1;
    const size_t index = max_code < 2 ? size_t(++max_code) : 0;
    heap.node[heap.len] = uint32_t(index);
    tree[index].freq = 1;
    heap.depth[index] = 0;
    opt_len -= 1;
    if (stree_len) static_len -= int64_t(stree_len[index]);
  }
  *out_max_code = size_t(max_code);
  for (size_t n = heap.len / 2; n >= 1; --n) heap.Down(tree, n);
  heap.Construct(tree, elements);
  uint16_t bl_count[kMaxBits + 1];
  GenBitLen(heap, tree, *out_max_code, stree_len, extra, extra_base, max_length,
            &opt_len, &static_len, bl_count);
  GenCodes(tree, *out_max_code, bl_count);
}

size_t Deflater::BuildBlTree() {
  ScanTree(bltree, ltree, l_max_code);
  ScanTree(bltree, dtree, d_max_code);
  size_t bl_max_code = 0;
  BuildTree(bltree, kBlCodes, nullptr, kExtraBlBits, 0, kMaxBlBits, &bl_max_code);
  size_t max_blindex = kBlCodes - 1;
  while (max_blindex >= 3) {
    if (bltree[kBlOrder[max_blindex]].len != 0) break;
    max_blindex -= 1;
  }
  opt_len += int64_t(3 * (max_blindex + 1) + 5 + 5 + 4);
  return max_blindex;
}
}  // namespace ruyijs::web_canvas::detail
