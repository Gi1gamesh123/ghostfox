#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace ruyijs::domain::canvas {

/// The donor build's canvas fingerprint configuration (dom/canvas/CanvasFingerprintConfig.h,
/// fields read from `--fpfile` as canvas.mode / seed / strength / density / preserveAlpha /
/// preserveWhitePoint / pngMetadata). Unconfigured, Firefox keeps its session-random key.
struct FingerprintConfig {
    enum class Mode : uint8_t { None, Pixel };
    Mode mode = Mode::None;
    std::optional<uint64_t> seed;
    uint8_t strength = 1;      // 0..3 (low/medium/high = 1/2/3)
    uint32_t density = 64;     // 1..4096: about 1/density of the pixels are perturbed
    bool preserveAlpha = true;
    bool preserveWhitePoint = true;
    bool pngMetadata = false;

    bool IsEnabled() const noexcept { return mode == Mode::Pixel && seed.has_value(); }
    bool ShouldPerturbPixels() const noexcept { return IsEnabled() && strength > 0; }
};

/// CanvasFingerprint.cpp MakeKey: FNV-1a over the configuration, expanded with SplitMix64.
inline std::array<uint8_t, 32> MakeFingerprintKey(const FingerprintConfig& config) noexcept {
    uint64_t hash = 1469598103934665603ULL;
    auto byte = [&hash](uint8_t value) {
        hash ^= value;
        hash *= 1099511628211ULL;
    };
    for (const unsigned char c : std::string("canvas-fingerprint")) byte(c);
    byte(0);
    const uint64_t seed = config.seed.value_or(0);
    for (uint8_t i = 0; i < 8; ++i) byte(static_cast<uint8_t>(seed >> (i * 8)));
    byte(static_cast<uint8_t>(config.mode));
    byte(config.strength);
    for (uint8_t i = 0; i < 4; ++i) byte(static_cast<uint8_t>(config.density >> (i * 8)));
    std::array<uint8_t, 32> key = {};
    uint64_t state = hash;
    for (size_t i = 0; i < 4; ++i) {
        uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        z ^= z >> 31;
        for (size_t j = 0; j < 8; ++j) key[i * 8 + j] = static_cast<uint8_t>(z >> (j * 8));
    }
    return key;
}

/// CanvasFingerprint.cpp PerturbPixels for RGBA bytes: a keyed SplitMix64 noise
/// per logical pixel (origin + x, origin + y * yDirection) replaces the low
/// `strength` bits of R, G, B (and A unless preserveAlpha) on about 1/density of
/// the pixels, with the density tightened so at least 8 pixels are expected to
/// change; transparent and opaque-white pixels are kept when preserveWhitePoint.
inline void PerturbRGBA(const FingerprintConfig& config, const std::array<uint8_t, 32>& key,
                        uint8_t* data, uint32_t width, uint32_t height, int32_t stride,
                        int32_t originX, int32_t originY, int32_t yDirection) noexcept {
    if (!data || !config.ShouldPerturbPixels()) return;
    auto word = [&key](size_t offset) {
        uint64_t value = 0;
        for (size_t i = 0; i < 8; ++i)
            value |= uint64_t(key[(offset + i) % key.size()]) << (i * 8);
        return value;
    };
    const uint64_t key0 = word(0), key1 = word(8);
    const uint64_t area = uint64_t(width) * uint64_t(height);
    const uint32_t density = std::max<uint32_t>(
        1u, std::min<uint32_t>(config.density,
                               uint32_t(std::max<uint64_t>(1, area / 8))));
    const uint8_t mask = uint8_t((1u << config.strength) - 1u);
    auto channel = [mask](uint8_t* value, uint64_t noise, unsigned shift) {
        *value = uint8_t((*value & ~mask) | (uint8_t(noise >> shift) & mask));
    };
    for (uint32_t y = 0; y < height; ++y) {
        uint8_t* row = data + std::ptrdiff_t(stride) * y;
        const uint64_t logicalY = uint64_t(int64_t(originY) + int64_t(y) * yDirection);
        for (uint32_t x = 0; x < width; ++x) {
            uint8_t* pixel = row + x * 4;
            if (config.preserveWhitePoint &&
                (pixel[3] == 0 ||
                 (pixel[3] == 255 && pixel[0] == 255 && pixel[1] == 255 && pixel[2] == 255)))
                continue;
            const uint64_t logicalX = uint64_t(int64_t(originX) + x);
            uint64_t state = key0 ^ (key1 + logicalX * 0xbf58476d1ce4e5b9ULL) ^
                             (logicalY * 0x94d049bb133111ebULL);
            uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
            const uint64_t noise = z ^ (z >> 31);
            if (density > 1 && (noise >> 40) % density != 0) continue;
            channel(pixel, noise, 0);
            channel(pixel + 1, noise, 8);
            channel(pixel + 2, noise, 16);
            if (!config.preserveAlpha) channel(pixel + 3, noise, 24);
        }
    }
}

}  // namespace ruyijs::domain::canvas
