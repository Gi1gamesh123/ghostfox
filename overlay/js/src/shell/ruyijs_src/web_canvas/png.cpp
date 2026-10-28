// The PNG writer. Every choice here is Firefox's: nsPNGEncoder hands libpng
// RGBA scanlines at zlib level 3, memLevel 8 and the "filtered" strategy, and
// libpng filters each row (sub, or none for a one-pixel-wide image) before
// deflating. The chunks are then laid out as libpng lays them out, including
// its 8 KiB IDAT chunk size and the randomization "deBG" chunk nsPNGEncoder puts
// after IDAT, so the bytes match Firefox exactly.
#include "web_canvas/internal.h"

#include "web_canvas/png_deflate.h"

namespace ruyijs::web_canvas::detail {
namespace {
// libpng's default compression buffer, which also bounds one IDAT chunk.
constexpr size_t kIdatChunkSize = 8192;

uint32_t Crc32(const uint8_t* data, size_t length) {
  uint32_t table[256];
  for (uint32_t index = 0; index < 256; ++index) {
    uint32_t value = index;
    for (int bit = 0; bit < 8; ++bit)
      value = (value & 1) ? (0xEDB88320u ^ (value >> 1)) : (value >> 1);
    table[index] = value;
  }
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t index = 0; index < length; ++index)
    crc = table[(crc ^ data[index]) & 0xFF] ^ (crc >> 8);
  return crc ^ 0xFFFFFFFFu;
}

void PushBE32(std::string* out, uint32_t value) {
  out->push_back(char((value >> 24) & 0xFF));
  out->push_back(char((value >> 16) & 0xFF));
  out->push_back(char((value >> 8) & 0xFF));
  out->push_back(char(value & 0xFF));
}

void Chunk(std::string* out, const char* type, const std::string& data) {
  PushBE32(out, uint32_t(data.size()));
  std::string body(type, 4);
  body += data;
  out->append(body);
  PushBE32(out, Crc32(reinterpret_cast<const uint8_t*>(body.data()), body.size()));
}

// libpng's own filtering: the default is "sub" per row, and a one-pixel-wide
// image drops sub (leaving no filter at all) because it could not improve.
std::string FilteredScanlines(const uint8_t* pixels, uint32_t width, uint32_t height) {
  const size_t stride = size_t(width) * 4u;
  std::string raw;
  raw.reserve(size_t(height) * (stride + 1));
  for (uint32_t row = 0; row < height; ++row) {
    const uint8_t* source = pixels + size_t(row) * stride;
    if (width == 1) {
      raw.push_back('\0');
      raw.append(reinterpret_cast<const char*>(source), stride);
      continue;
    }
    raw.push_back('\x01');
    for (size_t index = 0; index < stride; ++index) {
      const uint8_t left = index >= 4 ? source[index - 4] : 0;
      raw.push_back(char(uint8_t(source[index] - left)));
    }
  }
  return raw;
}

// libpng shrinks the window bits of the zlib stream to the data it compressed:
// png_deflate_claim picks them from the size, optimize_cmf rewrites the header.
int DeflateWindowBits(size_t raw_size) {
  int window_bits = 15;
  if (raw_size > 16384) return window_bits;
  unsigned half = 1u << (window_bits - 1);
  while (raw_size + 262 <= half) {
    half >>= 1;
    --window_bits;
  }
  return window_bits;
}

void OptimizeCmf(std::string* stream, size_t raw_size) {
  if (raw_size > 16384) return;
  unsigned cmf = uint8_t((*stream)[0]);
  if ((cmf & 0x0f) != 8 || (cmf & 0xf0) > 0x70) return;
  unsigned cinfo = cmf >> 4;
  unsigned half = 1u << (cinfo + 7);
  if (raw_size > half) return;
  do {
    half >>= 1;
    --cinfo;
  } while (cinfo > 0 && raw_size <= half);
  cmf = (cmf & 0x0f) | (cinfo << 4);
  (*stream)[0] = char(cmf);
  unsigned check = uint8_t((*stream)[1]) & 0xe0;
  check += 0x1f - ((cmf << 8) + check) % 0x1f;
  (*stream)[1] = char(check);
}
}  // namespace

std::string EncodePNG(const uint8_t* pixels, uint32_t width, uint32_t height,
                      const std::string& metadataKey) {
  std::string png("\x89PNG\r\n\x1a\n", 8);
  std::string header;
  PushBE32(&header, width);
  PushBE32(&header, height);
  header.push_back(8);  // bit depth
  header.push_back(6);  // colour type: truecolour with alpha
  header.push_back(0);  // deflate
  header.push_back(0);  // adaptive filtering
  header.push_back(0);  // no interlacing
  Chunk(&png, "IHDR", header);
  const std::string raw = FilteredScanlines(pixels, width, height);
  std::string zlib = Deflater(reinterpret_cast<const uint8_t*>(raw.data()), raw.size(),
                              DeflateWindowBits(raw.size())).Run();
  OptimizeCmf(&zlib, raw.size());
  for (size_t offset = 0; offset < zlib.size(); offset += kIdatChunkSize) {
    const size_t take = zlib.size() - offset < kIdatChunkSize ? zlib.size() - offset : kIdatChunkSize;
    Chunk(&png, "IDAT", zlib.substr(offset, take));
  }
  // nsPNGEncoder::MaybeAddCustomMetadata: the hash runs over the bytes written so
  // far (signature, IHDR and every IDAT), and the chunk goes after IDAT, before
  // IEND, exactly where png_write_end puts an unknown chunk.
  const std::string metadata = PngMetadataHex(metadataKey, png);
  if (!metadata.empty()) Chunk(&png, "deBG", metadata);
  Chunk(&png, "IEND", std::string());
  return png;
}

std::string Base64(const std::string& bytes) {
  static const char kAlphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((bytes.size() + 2) / 3) * 4);
  size_t index = 0;
  for (; index + 3 <= bytes.size(); index += 3) {
    const uint32_t value = (uint32_t(uint8_t(bytes[index])) << 16) |
                           (uint32_t(uint8_t(bytes[index + 1])) << 8) |
                           uint32_t(uint8_t(bytes[index + 2]));
    out.push_back(kAlphabet[(value >> 18) & 63]);
    out.push_back(kAlphabet[(value >> 12) & 63]);
    out.push_back(kAlphabet[(value >> 6) & 63]);
    out.push_back(kAlphabet[value & 63]);
  }
  const size_t remaining = bytes.size() - index;
  if (remaining == 1) {
    const uint32_t value = uint32_t(uint8_t(bytes[index])) << 16;
    out.push_back(kAlphabet[(value >> 18) & 63]);
    out.push_back(kAlphabet[(value >> 12) & 63]);
    out.push_back('=');
    out.push_back('=');
  } else if (remaining == 2) {
    const uint32_t value = (uint32_t(uint8_t(bytes[index])) << 16) |
                           (uint32_t(uint8_t(bytes[index + 1])) << 8);
    out.push_back(kAlphabet[(value >> 18) & 63]);
    out.push_back(kAlphabet[(value >> 12) & 63]);
    out.push_back(kAlphabet[(value >> 6) & 63]);
    out.push_back('=');
  }
  return out;
}
}  // namespace ruyijs::web_canvas::detail
