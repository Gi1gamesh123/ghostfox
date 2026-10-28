// One glyph mask through IDWriteGlyphRunAnalysis, exactly as SkScalerContext_DW
// asks for it: the same em size, measuring mode, grid fitting and grayscale
// antialiasing, the subpixel offset folded into the glyph run transform, and
// Skia's correcting table applied to the texture before the canvas blends it.
#include <cstring>

#include <dwrite_2.h>

#include "web_geometry/font_backend.h"

namespace ruyijs::web_geometry {
namespace {
ComPtr<IDWriteFactory2> SharedFactory2() {
  static const ComPtr<IDWriteFactory2> factory = [] {
    ComPtr<IDWriteFactory> base;
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                        reinterpret_cast<IUnknown**>(base.GetAddressOf()));
    ComPtr<IDWriteFactory2> factory2;
    if (base) base.As(&factory2);
    return factory2;
  }();
  return factory;
}
// GlyphMaskTable wants the quantised gamma and contrast SkScalerContextRec keeps.
float InternalGamma(float gamma) { return float(int(gamma * 64.0f)) / 64.0f; }
float InternalContrast(float contrast) { return float(int(contrast * 255.0f + 0.5f)) / 255.0f; }
}  // namespace

bool GlyphMask(const SelectedFont& font, float deviceSize, const float linear[4], float dx,
               float dy, uint16_t glyph, const uint8_t rgb[3], CanvasGlyphMask* out) {
  out->coverage.clear();
  const GlyphRenderStyle style = GlyphStyle(font.face.Get(), deviceSize);
  ComPtr<IDWriteFactory2> factory2 = SharedFactory2();
  ComPtr<IDWriteFontFace2> face2;
  font.face.As(&face2);
  FLOAT advance = 0;
  DWRITE_GLYPH_OFFSET offset{0, 0};
  DWRITE_GLYPH_RUN run{};
  run.fontFace = font.face.Get();
  run.fontEmSize = style.em;
  run.glyphCount = 1;
  run.glyphIndices = &glyph;
  run.glyphAdvances = &advance;
  run.glyphOffsets = &offset;
  DWRITE_MATRIX transform{linear[0], linear[1], linear[2], linear[3], dx, dy};
  ComPtr<IDWriteGlyphRunAnalysis> analysis;
  HRESULT hr;
  if (factory2 && face2) {
    // The extended entry point is the one that carries grid fitting and the
    // grayscale antialias mode; the donor only falls back without DirectWrite 2.
    hr = factory2->CreateGlyphRunAnalysis(&run, &transform,
                                          DWRITE_RENDERING_MODE(style.render),
                                          DWRITE_MEASURING_MODE(style.measure),
                                          DWRITE_GRID_FIT_MODE_ENABLED,
                                          DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, 0, 0,
                                          analysis.GetAddressOf());
  } else {
    hr = E_FAIL;
  }
  if (FAILED(hr) || !analysis) return false;
  RECT box{};
  if (FAILED(analysis->GetAlphaTextureBounds(DWRITE_TEXTURE_ALIASED_1x1, &box))) return false;
  const int width = int(box.right - box.left), height = int(box.bottom - box.top);
  if (width <= 0 || height <= 0) return true;
  out->coverage.assign(size_t(width) * size_t(height), 0);
  if (FAILED(analysis->CreateAlphaTexture(DWRITE_TEXTURE_ALIASED_1x1, &box, out->coverage.data(),
                                          UINT32(out->coverage.size()))))
    return false;
  uint8_t table[256];
  GlyphMaskTable(InternalGamma(SystemTextGamma()), InternalContrast(SystemTextContrast()), rgb,
                 table);
  for (uint8_t& value : out->coverage) value = table[value];
  out->left = int(box.left);
  out->top = int(box.top);
  out->width = width;
  out->height = height;
  return true;
}
}  // namespace ruyijs::web_geometry
