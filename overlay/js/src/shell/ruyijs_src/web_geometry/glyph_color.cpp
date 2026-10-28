// COLR glyphs: gfxFont::RenderColorGlyph and COLRFonts::PaintGlyphLayers. Firefox
// does not hand a COLRv0 glyph to Skia's colour scaler on Windows; it reads the
// BaseGlyphRecord itself and fills each LayerRecord glyph as an ordinary text
// glyph in its CPAL palette-0 colour. Up to an adjusted size of 256 the layers
// go into a 2x snapshot sized by the font's head bbox (gfxFontEntry::
// GetFontExtents, rounded out), which the canvas owner resamples; larger glyphs
// are filled straight onto the target. This file only produces the layer masks.
#include <dwrite_2.h>

#include <cmath>
#include <vector>

#include "web_geometry/font_backend.h"

namespace ruyijs::web_geometry {
namespace {
uint16_t U16(const uint8_t* p) { return uint16_t((p[0] << 8) | p[1]); }
uint32_t U32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
// COLRFonts::GetGlyphLayers for a version-0 table: the (glyph, palette index)
// LayerRecords of |glyph|'s BaseGlyphRecord, in paint order.
bool ColrLayers(IDWriteFontFace* face, uint16_t glyph,
                std::vector<std::pair<uint16_t, uint16_t>>* layers) {
  const void* data = nullptr;
  UINT32 size = 0;
  void* context = nullptr;
  BOOL exists = FALSE;
  if (FAILED(face->TryGetFontTable(DWRITE_MAKE_OPENTYPE_TAG('C', 'O', 'L', 'R'), &data, &size,
                                   &context, &exists)) || !exists)
    return false;
  const auto* colr = static_cast<const uint8_t*>(data);
  bool found = false;
  if (size >= 14 && U16(colr) == 0) {
    const uint16_t bases = U16(colr + 2);
    const uint32_t baseOffset = U32(colr + 4), layerOffset = U32(colr + 8);
    const uint16_t layerCount = U16(colr + 12);
    for (uint32_t i = 0; i < bases; ++i) {
      const uint64_t at = uint64_t(baseOffset) + uint64_t(i) * 6u;
      if (at + 6 > size) break;
      if (U16(colr + at) != glyph) continue;
      const uint32_t first = U16(colr + at + 2), count = U16(colr + at + 4);
      if (first + count > layerCount ||
          uint64_t(layerOffset) + uint64_t(first + count) * 4u > size) break;
      for (uint32_t k = 0; k < count; ++k) {
        const uint8_t* record = colr + layerOffset + (first + k) * 4u;
        layers->emplace_back(U16(record), U16(record + 2));
      }
      found = count != 0;
      break;
    }
  }
  face->ReleaseFontTable(context);
  return found;
}
}  // namespace

bool GlyphColorMask(const SelectedFont& font, float emSize, bool sampled, const float linear[4],
                    float dx, float dy, uint16_t glyph, const uint8_t foreground[3],
                    CanvasGlyphMask* out) {
  std::vector<std::pair<uint16_t, uint16_t>> records;
  if (!ColrLayers(font.face.Get(), glyph, &records)) return false;
  std::vector<uint32_t> palette;
  const bool havePalette = GlyphPalette(font.face.Get(), &palette);
  out->coverage.clear();
  out->layers.clear();
  out->sampled = sampled;
  out->left = out->top = out->width = out->height = 0;
  const float identity[4] = {1, 0, 0, 1};
  float size = emSize;
  if (sampled) {
    ComPtr<IDWriteFontFace1> face1;
    font.face.As(&face1);
    if (!face1) return false;
    DWRITE_FONT_METRICS1 metrics{};
    face1->GetMetrics(&metrics);
    if (!metrics.designUnitsPerEm) return false;
    // gfxFont::mFUnitsConvFactor and Rect::RoundOut of the head bbox.
    const float f = float(emSize / float(metrics.designUnitsPerEm));
    const float x = float(metrics.glyphBoxLeft) * f, y = float(-metrics.glyphBoxTop) * f;
    const float w = float(metrics.glyphBoxRight - metrics.glyphBoxLeft) * f;
    const float h = float(metrics.glyphBoxTop - metrics.glyphBoxBottom) * f;
    out->left = int(std::floor(x));
    out->top = int(std::floor(y));
    out->width = int(std::ceil(x + w)) - out->left;
    out->height = int(std::ceil(y + h)) - out->top;
    if (out->width <= 0 || out->height <= 0) return false;
    size = emSize * 2.0f;
  }
  for (const auto& record : records) {
    // PaintState::GetColor: palette entry, currentColor for 0xffff, else 25% black.
    uint8_t rgba[4] = {0, 0, 0, 64};
    if (havePalette && record.second < palette.size()) {
      const uint32_t c = palette[record.second];
      rgba[0] = uint8_t(c >> 16); rgba[1] = uint8_t(c >> 8); rgba[2] = uint8_t(c);
      rgba[3] = uint8_t(c >> 24);
    } else if (record.second == 0xffff) {
      rgba[0] = foreground[0]; rgba[1] = foreground[1]; rgba[2] = foreground[2]; rgba[3] = 255;
    }
    CanvasGlyphMask mask;
    if (!GlyphMask(font, size, sampled ? identity : linear, sampled ? 0.0f : dx,
                   sampled ? 0.0f : dy, record.first, rgba, &mask))
      return false;
    if (mask.coverage.empty()) continue;
    CanvasGlyphMask::Layer layer;
    // The snapshot draws the glyph at -bounds.TopLeft() under a scale of 2.
    layer.left = mask.left - (sampled ? 2 * out->left : 0);
    layer.top = mask.top - (sampled ? 2 * out->top : 0);
    layer.width = mask.width;
    layer.height = mask.height;
    layer.coverage = std::move(mask.coverage);
    for (int i = 0; i < 4; ++i) layer.rgba[i] = rgba[i];
    out->layers.push_back(std::move(layer));
  }
  return true;
}
}  // namespace ruyijs::web_geometry
