/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// Bounded table checks from gfxDWriteFont::HasBitmapStrikeForSize and IsCJKFont.
#pragma once
#include <cmath>
#include <cstdint>
#include "web_geometry/font_backend.h"
namespace ruyijs::web_geometry {

inline uint32_t Big32(const uint8_t* p) {
  return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}
inline unsigned Big16(const uint8_t* p) { return unsigned(p[0]) << 8 | p[1]; }
inline bool CJKFont(hb_face_t* face) {
  auto* blob = hb_face_reference_table(face, HB_TAG('O', 'S', '/', '2'));
  unsigned length;
  const auto* p = reinterpret_cast<const uint8_t*>(hb_blob_get_data(blob, &length));
  const bool result = length >= 86 && (Big32(p + 78) & (31u << 17));
  hb_blob_destroy(blob); return result;
}
inline bool BitmapSize(hb_face_t* face, unsigned size) {
  for (bool scaling : {false, true}) {
    auto* blob = hb_face_reference_table(face, scaling ? HB_TAG('E', 'B', 'S', 'C') : HB_TAG('E', 'B', 'L', 'C'));
    unsigned length;
    const auto* p = reinterpret_cast<const uint8_t*>(hb_blob_get_data(blob, &length));
    const unsigned stride = scaling ? 28 : 48, offset = scaling ? 24 : 44;
    bool found = false;
    if (length >= 8 && Big32(p) == 0x20000) {
      const auto count = Big32(p + 4);
      if (count <= 0xffff && count <= (length - 8) / stride) {
        for (unsigned i = 0; i < count; ++i) {
          const auto* item = p + 8 + i * stride;
          if (item[offset] == size && item[offset + 1] == size) {
            found = scaling || Big16(item + 42) >= Big16(item + 40) + 3;
            break;
          }
        }
      }
    }
    hb_blob_destroy(blob);
    if (found) return true;
  }
  return false;
}
#ifdef _WIN32
struct BitmapAdvance {
  IDWriteFontFace* face;
  float size, fontUnits;
};
inline hb_position_t GdiAdvance(hb_font_t*, void* data, hb_codepoint_t glyph, void*) {
  const auto& input = *static_cast<BitmapAdvance*>(data);
  const UINT16 id = UINT16(glyph);
  DWRITE_GLYPH_METRICS metrics{};
  if (FAILED(input.face->GetGdiCompatibleGlyphMetrics(std::round(input.size), 1,
      nullptr, FALSE, &id, 1, &metrics, FALSE))) return 0;
  return hb_position_t(std::floor(metrics.advanceWidth * input.fontUnits + .5) * 65536);
}
#endif

}
