// The deflate stream Firefox's PNG encoder emits. nsPNGEncoder drives libpng,
// which drives zlib at level 3 with memLevel 8 and (for filtered rows) the
// Z_FILTERED strategy; the Firefox binary links zlib-rs, whose level 3 runs its
// "medium" match finder. Reproducing the PNG byte for byte therefore needs that
// exact match finder, symbol buffer and Huffman tree construction, which is what
// this pure domain unit is: no JS, runtime, platform or compression dependency.
#ifndef RUYIJS_SRC_WEB_CANVAS_PNG_DEFLATE_H_
#define RUYIJS_SRC_WEB_CANVAS_PNG_DEFLATE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ruyijs::web_canvas::detail {

// LZ77 constants shared with zlib.
constexpr size_t kMinMatch = 3;
constexpr size_t kMaxMatch = 258;
constexpr size_t kWantMinMatch = 4;  // shortest match the medium parser emits
constexpr size_t kMinLookahead = kMaxMatch + kMinMatch + 1;  // 262
constexpr size_t kHashSize = 65536;  // zlib-rs hashes into 16 bits
constexpr size_t kLitBufSize = 1u << 14;  // memLevel 8
constexpr size_t kSymLimit = kLitBufSize - 1;  // 16383 symbols per block
constexpr size_t kLCodes = 286;  // literals + END_BLOCK + length codes
constexpr size_t kDCodes = 30;
constexpr size_t kBlCodes = 19;
constexpr size_t kHeapSize = 2 * kLCodes + 1;
constexpr size_t kMaxBits = 15;
constexpr size_t kMaxBlBits = 7;
constexpr size_t kRep3To6 = 16;
constexpr size_t kRepZ3To10 = 17;
constexpr size_t kRepZ11To138 = 18;

// One node of a Huffman tree. zlib overlays frequency with code and "dad" with
// length; separate fields keep the same order of writes readable.
struct TreeNode {
  uint16_t freq = 0;
  uint16_t dad = 0;
  uint16_t len = 0;
  uint16_t code = 0;
};

// The node heap one Huffman tree is built with (png_deflate_heap.cpp).
struct HuffmanHeap {
  uint32_t node[kHeapSize] = {};
  uint32_t depth[kHeapSize] = {};
  size_t len = 0;
  size_t max = 0;
  int64_t Initialize(TreeNode* tree, size_t elements);
  uint32_t Key(const TreeNode* tree, uint32_t index) const;
  void Down(const TreeNode* tree, size_t k);
  uint32_t Remove(const TreeNode* tree);
  void Construct(TreeNode* tree, size_t first_node);
};

// deflate writes codes least significant bit first into a byte stream.
struct BitWriter {
  std::string out;
  uint64_t buffer = 0;
  unsigned used = 0;
  void Send(uint64_t value, unsigned length);
  void Align();
};

// Extra bits and base values of the length, distance and bit-length codes, and
// the order the bit-length code lengths travel in (all from RFC 1951).
constexpr uint8_t kExtraLBits[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                     2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr uint8_t kExtraDBits[30] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                     6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
constexpr uint8_t kExtraBlBits[19] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                      0, 0, 0, 0, 0, 0, 2, 3, 7};
constexpr uint8_t kBaseLength[29] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 20, 24,
                                     28, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192,
                                     224, 0};
constexpr uint16_t kBaseDist[30] = {0, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128,
                                    192, 256, 384, 512, 768, 1024, 1536, 2048, 3072, 4096,
                                    6144, 8192, 12288, 16384, 24576};
constexpr uint8_t kBlOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5,
                                  11, 4, 12, 3, 13, 2, 14, 1, 15};

// The tables deflate derives once from the compressed format description.
struct DeflateTables {
  uint8_t length_code[kMaxMatch - kMinMatch + 1] = {};
  uint8_t dist_code[512] = {};
  uint16_t static_lcode[288] = {};
  uint16_t static_llen[288] = {};
  uint16_t static_dcode[30] = {};
  uint16_t static_dlen[30] = {};
  DeflateTables();
};
const DeflateTables& Tables();
// zlib's d_code(): the distance code of a zero-based distance.
inline uint8_t DistCode(size_t dist) {
  if (dist < 256) return Tables().dist_code[dist];
  return Tables().dist_code[256 + (dist >> 7)];
}

// The one deflater: the whole input is compressed in a single Z_FINISH pass, so
// there is no pending-output or resume state, only the LZ77 window, the symbol
// buffer of the current block and the Huffman trees.
struct Deflater {
  Deflater(const uint8_t* data, size_t size, int window_bits);

  // The complete zlib stream: header, blocks, byte alignment and adler32.
  std::string Run();

  // png_deflate_window.cpp: window filling, sliding, hash chains and matching.
  void FillWindow();
  void SlideHash();
  uint16_t QuickInsert(size_t string);
  void InsertStrings(size_t string, size_t count);
  size_t LongestMatch(size_t cur_match, size_t* out_match_start);

  // png_deflate_medium.cpp: the level-3 parser and the symbol buffer.
  void Medium();
  void InsertMatch(size_t orgstart, size_t length);
  bool TallyLiteral(uint8_t literal);
  bool TallyMatch(size_t dist, size_t length);
  bool ShouldFlush() const { return symbols == kSymLimit; }

  // png_deflate_tree.cpp: Huffman construction and the bit-length tree.
  void BuildTree(TreeNode* tree, size_t elements, const uint16_t* static_len,
                 const uint8_t* extra, size_t extra_base, size_t max_length,
                 size_t* out_max_code);
  size_t BuildBlTree();

  // png_deflate_codes.cpp: sending the trees and the block body.
  void SendAllTrees(size_t lcodes, size_t dcodes, size_t blcodes);
  void SendTree(const TreeNode* tree, size_t max_code);
  void CompressBlockDynamic();
  void CompressBlockStatic();

  // png_deflate_block.cpp: the block, its type and the symbol buffer reset.
  void FlushBlock(bool last);
  void TrFlushBlock(int64_t window_offset, size_t stored_len, bool last);
  void StoredBlock(size_t offset, size_t length, bool last);
  void EmitTree(unsigned type, bool last) { bits.Send((uint64_t(type) << 1) | (last ? 1u : 0u), 3); }
  void InitBlock();

  // Input and window.
  const uint8_t* source = nullptr;
  size_t source_size = 0;
  const uint8_t* in = nullptr;
  size_t avail_in = 0;
  int w_bits = 15;
  std::vector<uint8_t> window;  // 2 * w_size plus kMaxMatch zeros of slack
  std::vector<uint16_t> head;   // kHashSize
  std::vector<uint16_t> prev;   // w_size
  size_t w_size = 0;
  size_t window_size = 0;
  size_t max_dist = 0;
  size_t lookahead = 0;
  size_t strstart = 0;
  int64_t block_start = 0;
  size_t insert = 0;
  size_t match_start = 0;
  uint16_t prev_length = 0;
  // Level-3 parameters from zlib-rs' configuration table.
  uint16_t max_chain = 6;
  uint16_t max_lazy = 6;
  uint16_t good_match = 4;
  uint16_t nice_match = 16;

  // Symbol buffer: three bytes (little-endian distance, length byte) per symbol.
  std::vector<uint8_t> sym;
  size_t symbols = 0;

  // The three trees of the current block.
  TreeNode ltree[kHeapSize];
  TreeNode dtree[2 * kDCodes + 1];
  TreeNode bltree[2 * kBlCodes + 1];
  size_t l_max_code = 0;
  size_t d_max_code = 0;
  int64_t opt_len = 0;
  int64_t static_len = 0;
  BitWriter bits;
};
}  // namespace ruyijs::web_canvas::detail
#endif
