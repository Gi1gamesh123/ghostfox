// Sending the deflate block body: the code-length tree, the two Huffman trees of
// a dynamic block, and the literal, length and distance codes of each symbol.
#include "web_canvas/png_deflate.h"

namespace ruyijs::web_canvas::detail {
namespace {
void SendLiteral(BitWriter& bits, const TreeNode* ltree, uint8_t literal) {
  const TreeNode& node = ltree[literal];
  bits.Send(node.code, node.len);
}

void SendLengthAndDistance(BitWriter& bits, const TreeNode* ltree, const TreeNode* dtree,
                           uint8_t length_code, uint16_t dist) {
  const size_t code = Tables().length_code[length_code];
  const TreeNode& lnode = ltree[code + 257];
  uint64_t value = lnode.code;
  unsigned size = lnode.len;
  const unsigned extra = kExtraLBits[code];
  if (extra != 0) {
    value |= uint64_t(length_code - kBaseLength[code]) << size;
    size += extra;
  }
  dist = uint16_t(dist - 1);
  const size_t dcode = DistCode(dist);
  const TreeNode& dnode = dtree[dcode];
  value |= uint64_t(dnode.code) << size;
  size += dnode.len;
  const unsigned d_extra = kExtraDBits[dcode];
  if (d_extra != 0) {
    value |= uint64_t(dist - kBaseDist[dcode]) << size;
    size += d_extra;
  }
  bits.Send(value, size);
}
}  // namespace

// zlib's send_tree: emit one tree's code lengths through the bit-length tree,
// with the run-length codes 16, 17 and 18.
void Deflater::SendTree(const TreeNode* tree, size_t max_code) {
  int64_t previous = -1;
  unsigned next = tree[0].len;
  size_t count = 0;
  size_t max_count = 7;
  size_t min_count = 4;
  if (next == 0) {
    max_count = 138;
    min_count = 3;
  }
  for (size_t n = 0; n <= max_code; ++n) {
    const unsigned current = next;
    next = tree[n + 1].len;
    count += 1;
    if (count < max_count && int64_t(current) == int64_t(next)) continue;
    if (count < min_count) {
      while (count != 0) {
        const TreeNode& node = bltree[current];
        bits.Send(node.code, node.len);
        count -= 1;
      }
    } else if (current != 0) {
      if (int64_t(current) != previous) {
        const TreeNode& node = bltree[current];
        bits.Send(node.code, node.len);
        count -= 1;
      }
      const TreeNode& repeat = bltree[kRep3To6];
      bits.Send(repeat.code, repeat.len);
      bits.Send(count - 3, 2);
    } else if (count <= 10) {
      const TreeNode& repeat = bltree[kRepZ3To10];
      bits.Send(repeat.code, repeat.len);
      bits.Send(count - 3, 3);
    } else {
      const TreeNode& repeat = bltree[kRepZ11To138];
      bits.Send(repeat.code, repeat.len);
      bits.Send(count - 11, 7);
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

void Deflater::SendAllTrees(size_t lcodes, size_t dcodes, size_t blcodes) {
  bits.Send(lcodes - 257, 5);
  bits.Send(dcodes - 1, 5);
  bits.Send(blcodes - 4, 4);
  for (size_t rank = 0; rank < blcodes; ++rank) bits.Send(bltree[kBlOrder[rank]].len, 3);
  SendTree(ltree, lcodes - 1);
  SendTree(dtree, dcodes - 1);
}

void Deflater::CompressBlockDynamic() {
  for (size_t index = 0; index < symbols; ++index) {
    const uint8_t* entry = &sym[index * 3];
    const uint16_t dist = uint16_t(entry[0] | (uint16_t(entry[1]) << 8));
    if (dist == 0) {
      SendLiteral(bits, ltree, entry[2]);
    } else {
      SendLengthAndDistance(bits, ltree, dtree, entry[2], dist);
    }
  }
  const TreeNode& end = ltree[256];
  bits.Send(end.code, end.len);
}

void Deflater::CompressBlockStatic() {
  const DeflateTables& tables = Tables();
  for (size_t index = 0; index < symbols; ++index) {
    const uint8_t* entry = &sym[index * 3];
    const uint16_t dist = uint16_t(entry[0] | (uint16_t(entry[1]) << 8));
    if (dist == 0) {
      const uint8_t literal = entry[2];
      bits.Send(tables.static_lcode[literal], tables.static_llen[literal]);
      continue;
    }
    const size_t code = tables.length_code[entry[2]];
    uint64_t value = tables.static_lcode[code + 257];
    unsigned size = tables.static_llen[code + 257];
    const unsigned extra = kExtraLBits[code];
    if (extra != 0) {
      value |= uint64_t(entry[2] - kBaseLength[code]) << size;
      size += extra;
    }
    const uint16_t zero_based = uint16_t(dist - 1);
    const size_t dcode = DistCode(zero_based);
    value |= uint64_t(tables.static_dcode[dcode]) << size;
    size += 5;
    const unsigned d_extra = kExtraDBits[dcode];
    if (d_extra != 0) {
      value |= uint64_t(zero_based - kBaseDist[dcode]) << size;
      size += d_extra;
    }
    bits.Send(value, size);
  }
  bits.Send(tables.static_lcode[256], tables.static_llen[256]);
}
}  // namespace ruyijs::web_canvas::detail
