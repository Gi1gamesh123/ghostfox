// The deBG chunk's value: Firefox's nsRFPService::GenerateRandomizationKeyFromHash,
// ported as a pure function. The key is the 32 random bytes nsRFPService derives
// once per browsing session (nsICookieJarSettings::GetFingerprintingRandomizationKey);
// this Context generates its own once, from the same OS source the runtime's
// crypto owner uses. The hash is mozilla::HashBytes over the encoded PNG bytes,
// scrambled with SipHash-1-3 keyed by the first half of the key, then expanded
// through two xorshift128+ generators seeded from that digest.
#include "domain/canvas/fingerprint_config.h"
#include "web_canvas/internal.h"

#include <cstdio>
#include "mozilla/HashFunctions.h"
#include "mozilla/RandomNum.h"

namespace ruyijs::web_canvas::detail {
namespace {
constexpr uint64_t kSipInit0 = 0x736f6d6570736575ull;
constexpr uint64_t kSipInit1 = 0x646f72616e646f6dull;
constexpr uint64_t kSipInit2 = 0x6c7967656e657261ull;
constexpr uint64_t kSipInit3 = 0x7465646279746573ull;

uint64_t RotateLeft64(uint64_t value, int bits) {
  return (value << bits) | (value >> (64 - bits));
}

// mozilla::HashCodeScrambler: SipHash-1-3 over one 8-byte message.
uint64_t SipHash13(uint64_t aK0, uint64_t aK1, uint64_t message) {
  uint64_t v0 = aK0 ^ kSipInit0, v1 = aK1 ^ kSipInit1;
  uint64_t v2 = aK0 ^ kSipInit2, v3 = aK1 ^ kSipInit3;
  const auto round = [&] {
    v0 += v1;
    v1 = RotateLeft64(v1, 13);
    v1 ^= v0;
    v0 = RotateLeft64(v0, 32);
    v2 += v3;
    v3 = RotateLeft64(v3, 16);
    v3 ^= v2;
    v0 += v3;
    v3 = RotateLeft64(v3, 21);
    v3 ^= v0;
    v2 += v1;
    v1 = RotateLeft64(v1, 17);
    v1 ^= v2;
    v2 = RotateLeft64(v2, 32);
  };
  v3 ^= message;
  round();
  v0 ^= message;
  v2 ^= 0xff;
  round();
  round();
  round();
  return v0 ^ v1 ^ v2 ^ v3;
}

// mozilla::non_crypto::XorShift128PlusRNG.
struct XorShift128Plus {
  uint64_t state[2];
  XorShift128Plus(uint64_t first, uint64_t second) : state{first, second} {}
  uint64_t Next() {
    uint64_t s1 = state[0];
    const uint64_t s0 = state[1];
    state[0] = s0;
    s1 ^= s1 << 23;
    state[1] = s1 ^ s0 ^ (s1 >> 17) ^ (s0 >> 26);
    return state[1] + s0;
  }
};

// Little-endian word loads, as the reinterpret_cast in nsRFPService.cpp does on
// every platform this engine runs on.
uint64_t WordLE(const std::string& bytes, size_t offset) {
  uint64_t word = 0;
  for (size_t index = 8; index-- > 0;)
    word = (word << 8) | uint8_t(bytes[offset + index]);
  return word;
}
// nsPNGEncoder::WriteCallback chains mozilla::HashBytes over every libpng write:
// the signature, then per chunk its 8-byte length+type header, its data (only
// when non-empty, png_write_chunk_data) and its 4-byte CRC, each one call.
uint32_t WriteCallbackHash(const std::string& png) {
  const auto* bytes = reinterpret_cast<const uint8_t*>(png.data());
  uint32_t hash = 0;
  if (png.size() < 8) return mozilla::HashBytes(bytes, png.size(), hash);
  hash = mozilla::HashBytes(bytes, 8, hash);
  size_t offset = 8;
  while (offset + 12 <= png.size()) {
    const size_t length = (size_t(bytes[offset]) << 24) | (size_t(bytes[offset + 1]) << 16) |
                          (size_t(bytes[offset + 2]) << 8) | size_t(bytes[offset + 3]);
    if (offset + 12 + length > png.size()) break;
    hash = mozilla::HashBytes(bytes + offset, 8, hash);
    if (length) hash = mozilla::HashBytes(bytes + offset + 8, length, hash);
    hash = mozilla::HashBytes(bytes + offset + 8 + length, 4, hash);
    offset += 12 + length;
  }
  return hash;
}
}  // namespace

std::string PngMetadataHex(const std::string& key, const std::string& content) {
  // Firefox reads three 8-byte words out of the key and bails out on any other
  // shape; the session key is exactly 32 bytes.
  if (key.size() != 32) return std::string();
  const uint32_t contentHash = WriteCallbackHash(content);
  const uint64_t k2 = WordLE(key, 16);
  const uint64_t digest =
      static_cast<uint64_t>(uint32_t(SipHash13(WordLE(key, 0), WordLE(key, 8), contentHash))) << 32 |
      contentHash;
  std::string expanded(32, '\0');
  XorShift128Plus rng(digest, k2);
  for (size_t word = 0; word < 4; ++word) {
    const uint64_t value = rng.Next();
    for (size_t byte = 0; byte < 8; ++byte)
      expanded[word * 8 + byte] = char((value >> (byte * 8)) & 0xFF);
  }
  XorShift128Plus spread(WordLE(expanded, 0), WordLE(expanded, 8));
  char hex[17];
  std::snprintf(hex, sizeof(hex), "%016llX", static_cast<unsigned long long>(spread.Next()));
  return std::string(hex, 16);
}

namespace {
// One Context is one runtime process, so the configured fingerprint is
// process-scoped; every realm of the Context reads the same value.
domain::canvas::FingerprintConfig& Configured() {
  static domain::canvas::FingerprintConfig config;
  return config;
}
std::string& ConfiguredKey() {
  static std::string key;
  return key;
}
}  // namespace

const std::string* ConfiguredMetadataKey() {
  static const std::string kNone;
  if (!Configured().IsEnabled()) return nullptr;
  return Configured().pngMetadata ? &ConfiguredKey() : &kNone;
}

const std::string& PngMetadataKey(State& state) {
  if (state.pngMetadataKey.size() != 32) {
    std::string key(32, '\0');
    // A failed OS read leaves the key empty, which drops the chunk the same way
    // an unavailable cookie-jar key drops it in Firefox.
    if (!mozilla::GenerateRandomBytesFromOS(key.data(), key.size())) key.clear();
    state.pngMetadataKey = key;
  }
  return state.pngMetadataKey;
}
}  // namespace ruyijs::web_canvas::detail

namespace ruyijs::web_canvas {
void ConfigureFingerprint(const domain::canvas::FingerprintConfig& config) {
  detail::Configured() = config;
  const auto key = domain::canvas::MakeFingerprintKey(config);
  detail::ConfiguredKey().assign(reinterpret_cast<const char*>(key.data()), key.size());
}
void PerturbExportedPixels(uint8_t* rgba, uint32_t width, uint32_t height, int32_t stride,
                           int32_t originX, int32_t originY, int32_t yDirection) {
  const auto& config = detail::Configured();
  if (!config.ShouldPerturbPixels()) return;
  std::array<uint8_t, 32> key{};
  const std::string& bytes = detail::ConfiguredKey();
  for (size_t i = 0; i < key.size() && i < bytes.size(); ++i) key[i] = uint8_t(bytes[i]);
  domain::canvas::PerturbRGBA(config, key, rgba, width, height, stride, originX, originY,
                              yDirection);
}
}  // namespace ruyijs::web_canvas
