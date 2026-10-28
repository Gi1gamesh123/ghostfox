// The palette a colour font's COLR layers name: palette 0 of the CPAL table, the
// one Skia's DWriteFontTypeface keeps as its target palette.
#include <dwrite_2.h>

#include <cmath>
#include <vector>

#include "web_geometry/font_backend.h"

namespace ruyijs::web_geometry {
// The palette Skia reads once per typeface: palette 0 of the CPAL table.
bool GlyphPalette(IDWriteFontFace* face, std::vector<uint32_t>* colors) {
  ComPtr<IDWriteFontFace2> face2;
  face->QueryInterface(__uuidof(IDWriteFontFace2), reinterpret_cast<void**>(face2.GetAddressOf()));
  if (!face2) return false;
  const UINT32 count = face2->GetPaletteEntryCount();
  if (!count) return false;
  std::vector<DWRITE_COLOR_F> entries(count);
  if (FAILED(face2->GetPaletteEntries(0, 0, count, entries.data()))) return false;
  colors->resize(count);
  for (UINT32 i = 0; i < count; ++i) {
    const auto& c = entries[i];
    const auto byte = [](float v) {
      return uint8_t(v <= 0 ? 0 : v >= 1 ? 255 : int(std::floor(v * 255.0f + 0.5f)));
    };
    (*colors)[i] = (uint32_t(byte(c.a)) << 24) | (uint32_t(byte(c.r)) << 16) |
                   (uint32_t(byte(c.g)) << 8) | uint32_t(byte(c.b));
  }
  return true;
}
}  // namespace ruyijs::web_geometry
