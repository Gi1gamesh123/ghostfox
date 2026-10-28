#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ruyijs::domain::web_media {
// Runtime-independent decoded pixels; adapters own the codec and JS lifetime.
struct ImageFrame {
  uint32_t width = 0, height = 0;
  uint64_t timestamp = 0, duration = 0;
  bool animated = false, opaque = false;
  std::vector<uint8_t> pixels;  // Premultiplied BGRA, tightly packed.
};
struct ImageData {
  std::vector<ImageFrame> frames;
  double repetitions = 0;
  bool animated = false;
};
enum class ImageDecodeStatus { Ok, Unsupported, InvalidData, OutOfMemory };
inline constexpr uint64_t kImagePixelLimit = 64u * 1024u * 1024u;
inline constexpr size_t kEncodedImageLimit = 256u * 1024u * 1024u;
}
