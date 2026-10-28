#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace ruyijs::domain::font_loading {

inline uint16_t ReadU16BE(std::span<const uint8_t> bytes, size_t offset) noexcept {
    return static_cast<uint16_t>((static_cast<uint16_t>(bytes[offset]) << 8) |
                                 bytes[offset + 1]);
}

inline uint32_t ReadU32BE(std::span<const uint8_t> bytes, size_t offset) noexcept {
    return (static_cast<uint32_t>(bytes[offset]) << 24) |
           (static_cast<uint32_t>(bytes[offset + 1]) << 16) |
           (static_cast<uint32_t>(bytes[offset + 2]) << 8) |
           static_cast<uint32_t>(bytes[offset + 3]);
}

inline bool HasTag(std::span<const uint8_t> bytes,
                   char a, char b, char c, char d) noexcept {
    return bytes.size() >= 4 && bytes[0] == static_cast<uint8_t>(a) &&
           bytes[1] == static_cast<uint8_t>(b) &&
           bytes[2] == static_cast<uint8_t>(c) &&
           bytes[3] == static_cast<uint8_t>(d);
}

// Performs bounded container validation only. Font shaping and glyph parsing
// remain infrastructure capabilities and must not leak into this domain rule.
inline bool IsStructurallyLoadable(std::span<const uint8_t> bytes) noexcept {
    if (bytes.size() < 12) return false;

    if (HasTag(bytes, 'w', 'O', 'F', 'F')) {
        if (bytes.size() < 44 || ReadU32BE(bytes, 8) != bytes.size()) return false;
        const size_t table_count = ReadU16BE(bytes, 12);
        return table_count > 0 && 44 + table_count * 20 <= bytes.size();
    }
    if (HasTag(bytes, 'w', 'O', 'F', '2')) {
        if (bytes.size() < 48 || ReadU32BE(bytes, 8) != bytes.size()) return false;
        return ReadU16BE(bytes, 12) > 0;
    }
    if (HasTag(bytes, 't', 't', 'c', 'f')) {
        const size_t font_count = ReadU32BE(bytes, 8);
        return font_count > 0 && 12 + font_count * 4 <= bytes.size();
    }

    const bool sfnt = ReadU32BE(bytes, 0) == 0x00010000u ||
                      HasTag(bytes, 'O', 'T', 'T', 'O') ||
                      HasTag(bytes, 't', 'r', 'u', 'e') ||
                      HasTag(bytes, 't', 'y', 'p', '1');
    if (!sfnt) return false;
    const size_t table_count = ReadU16BE(bytes, 4);
    return table_count > 0 && 12 + table_count * 16 <= bytes.size();
}

} // namespace ruyijs::domain::font_loading
