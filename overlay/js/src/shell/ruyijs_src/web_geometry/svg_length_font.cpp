// Additional metrics from the original shared font collection, without a cache.
#include "web_geometry/font_backend.h"
#include <cmath>
namespace ruyijs::web_geometry {
bool SystemFonts::SvgLengthMetrics(const FontInput& input, float* ex, float* ch, float* ic, float* cap) {
  *ex = *ch = *ic = *cap = 0;
  if (input.size <= 0) return true;
  if (!collection_) {
    auto collection = std::make_unique<FontCollection>();
    if (!collection->Initialize()) return false;
    collection_ = std::move(collection);
  }
  SelectedFont font;
  if (!collection_->Select(input, 0x20, &font)) return false;
#ifdef _WIN32
  DWRITE_FONT_METRICS metrics{};
  font.face->GetMetrics(&metrics);
  if (!metrics.designUnitsPerEm) return false;
  const float size = float(std::round(input.size * 60) / 60);
  const float factor = size / metrics.designUnitsPerEm;
  *ex = float(std::round(metrics.xHeight * factor * 60) / 60);
  *cap = float(std::round(metrics.capHeight * factor * 60) / 60);
  const uint32_t codes[] = {0x30, 0x6c34};
  for (unsigned i = 0; i < 2; ++i) {
    uint16_t glyph = 0;
    DWRITE_GLYPH_METRICS gm{};
    if (FAILED(font.face->GetGlyphIndices(codes + i, 1, &glyph))) return false;
    float result = i ? size : size * .5f;
    if (glyph) {
      if (FAILED(font.face->GetDesignGlyphMetrics(&glyph, 1, &gm))) return false;
      result = float(std::round(gm.advanceWidth * factor * 60) / 60);
    }
    *(i ? ic : ch) = result;
  }
  if (*ex <= 0) *ex = size * .5f;
  if (*cap <= 0) *cap = size;
#endif
  return true;
}
}
