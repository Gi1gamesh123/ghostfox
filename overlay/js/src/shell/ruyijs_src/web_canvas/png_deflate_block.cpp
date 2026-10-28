// One deflate block: choosing between a stored, static-tree and dynamic-tree
// encoding, the stored form itself, and the symbol buffer reset after a block.
#include "web_canvas/png_deflate.h"

namespace ruyijs::web_canvas::detail {
namespace {
constexpr size_t kEndBlock = 256;
}  // namespace

void Deflater::InitBlock() {
  for (size_t index = 0; index < kLCodes; ++index) ltree[index].freq = 0;
  for (size_t index = 0; index < kDCodes; ++index) dtree[index].freq = 0;
  for (size_t index = 0; index < kBlCodes; ++index) bltree[index].freq = 0;
  ltree[kEndBlock].freq = 1;  // end of block literal code
  opt_len = 0;
  static_len = 0;
  for (uint8_t& byte : sym) byte = 0;
  symbols = 0;
}

void Deflater::StoredBlock(size_t offset, size_t length, bool last) {
  EmitTree(0, last);  // stored block
  bits.Align();
  const uint16_t stored = uint16_t(length);
  const uint16_t complement = uint16_t(~stored);
  bits.out.push_back(static_cast<char>(stored & 0xff));
  bits.out.push_back(static_cast<char>((stored >> 8) & 0xff));
  bits.out.push_back(static_cast<char>(complement & 0xff));
  bits.out.push_back(static_cast<char>((complement >> 8) & 0xff));
  for (size_t index = 0; index < length; ++index)
    bits.out.push_back(static_cast<char>(window[offset + index]));
}

// zlib's _tr_flush_block: build the trees, then keep the cheapest encoding.
void Deflater::TrFlushBlock(int64_t window_offset, size_t stored_len, bool last) {
  int64_t opt_lenb = 0;
  int64_t static_lenb = 0;
  size_t max_blindex = 0;
  if (symbols == 0) {
    static_len = 7;
  } else {
    BuildTree(ltree, kLCodes, Tables().static_llen, kExtraLBits, 257, kMaxBits, &l_max_code);
    BuildTree(dtree, kDCodes, Tables().static_dlen, kExtraDBits, 0, kMaxBits, &d_max_code);
    max_blindex = BuildBlTree();
    opt_lenb = (opt_len + 3 + 7) >> 3;
    static_lenb = (static_len + 3 + 7) >> 3;
    if (static_lenb <= opt_lenb) opt_lenb = static_lenb;
  }
  if (int64_t(stored_len) + 4 <= opt_lenb && window_offset >= 0) {
    StoredBlock(size_t(window_offset), stored_len, last);
  } else if (static_lenb == opt_lenb) {
    EmitTree(1, last);  // fixed Huffman trees
    CompressBlockStatic();
  } else {
    EmitTree(2, last);  // dynamic Huffman trees
    SendAllTrees(l_max_code + 1, d_max_code + 1, max_blindex + 1);
    CompressBlockDynamic();
  }
  InitBlock();
  if (last) bits.Align();
}

void Deflater::FlushBlock(bool last) {
  const int64_t stored_len = int64_t(strstart) - block_start;
  TrFlushBlock(block_start >= 0 ? block_start : -1,
               stored_len > 0 ? size_t(stored_len) : 0, last);
  block_start = int64_t(strstart);
}
}  // namespace ruyijs::web_canvas::detail
