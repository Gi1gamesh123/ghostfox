// The node heap zlib builds each Huffman tree with: frequency first, then the
// tree depth it breaks ties by. Same ordering, same ties, same codes.
#include "web_canvas/png_deflate.h"

namespace ruyijs::web_canvas::detail {
int64_t HuffmanHeap::Initialize(TreeNode* tree, size_t elements) {
  int64_t max_code = -1;
  len = 0;
  max = kHeapSize;
  for (size_t index = 0; index < elements; ++index) {
    if (tree[index].freq > 0) {
      len += 1;
      node[len] = uint32_t(index);
      max_code = int64_t(index);
      depth[index] = 0;
    } else {
      tree[index].len = 0;
    }
  }
  return max_code;
}

uint32_t HuffmanHeap::Key(const TreeNode* tree, uint32_t index) const {
  return (uint32_t(tree[index].freq) << 8) | depth[index];
}

void HuffmanHeap::Down(const TreeNode* tree, size_t k) {
  const uint32_t value = node[k];
  const uint32_t key = Key(tree, value);
  size_t child = k << 1;
  while (child <= len) {
    uint32_t child_key = Key(tree, node[child]);
    if (child < len) {
      const uint32_t sibling_key = Key(tree, node[child + 1]);
      if (sibling_key <= child_key) {
        child += 1;
        child_key = sibling_key;
      }
    }
    if (key <= child_key) break;
    node[k] = node[child];
    k = child;
    child <<= 1;
  }
  node[k] = value;
}

uint32_t HuffmanHeap::Remove(const TreeNode* tree) {
  const uint32_t top = node[1];
  node[1] = node[len];
  len -= 1;
  Down(tree, 1);
  return top;
}

// zlib's construct_huffman_tree: combine the two least frequent nodes, keeping
// the nodes sorted by frequency at the top of the heap array.
void HuffmanHeap::Construct(TreeNode* tree, size_t first_node) {
  size_t next = first_node;
  for (;;) {
    const size_t n = Remove(tree);
    const size_t m = node[1];
    max -= 1;
    node[max] = uint32_t(n);
    max -= 1;
    node[max] = uint32_t(m);
    tree[next].freq = uint16_t(tree[n].freq + tree[m].freq);
    depth[next] = (depth[n] > depth[m] ? depth[n] : depth[m]) + 1;
    tree[n].dad = uint16_t(next);
    tree[m].dad = uint16_t(next);
    node[1] = uint32_t(next);
    next += 1;
    Down(tree, 1);
    if (len < 2) break;
  }
  max -= 1;
  node[max] = node[1];
}
}  // namespace ruyijs::web_canvas::detail
