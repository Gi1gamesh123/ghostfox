// The glyph masks of one fillText. The run split is the shaping owner's (same
// fonts, same segments as the outline producer), and every glyph becomes the
// device-space mask Firefox hands to its mask blitter: the position is snapped
// to the nearest eighth of a pixel and the remainder becomes the DirectWrite
// glyph run offset, the way Skia fills a SkTextBlob position into a strike.
#include <cmath>

#include "web_geometry/font_backend.h"

namespace ruyijs::web_geometry {
namespace {
uint32_t Codepoint(const std::u16string& text, size_t* at) {
  uint32_t code = text[(*at)++];
  if (code >= 0xd800 && code <= 0xdbff && *at < text.size() &&
      text[*at] >= 0xdc00 && text[*at] <= 0xdfff)
    code = 0x10000 + ((code - 0xd800) << 10) + text[(*at)++] - 0xdc00;
  return code;
}
bool Continuation(uint32_t code) {
  return code == 0x200d || (code >= 0xfe00 && code <= 0xfe0f) ||
      (code >= 0xe0100 && code <= 0xe01ef) ||
      hb_unicode_combining_class(hb_unicode_funcs_get_default(), code) != 0;
}
// SkPackedGlyphID packing: add the 1/8 rounding constant, floor for the device
// integer, and take the remaining quarter-pixel steps as the subpixel field.
void SnapPosition(float position, int* base, float* subpixel) {
  const float mapped = position + 0.125f;
  *base = int(std::floor(mapped));
  const float fraction = mapped - float(*base);
  *subpixel = float(int(fraction * 4.0f)) * 0.25f;
}
}  // namespace

bool SystemFonts::BuildCanvasGlyphMasks(const std::u16string& text, const FontInput& input,
                                        const float matrix[6], const uint8_t rgb[3],
                                        std::vector<CanvasGlyphMask>* result) {
  result->clear();
  if (text.empty()) return true;
  if (text.size() > 65536 || !std::isfinite(input.scale) || input.scale <= 0 ||
      !std::isfinite(input.size) || input.size <= 0 || !std::isfinite(input.x) ||
      !std::isfinite(input.y)) return false;
  if (!collection_) {
    auto collection = std::make_unique<FontCollection>();
    if (!collection->Initialize()) return false;
    collection_ = std::move(collection);
  }
  const float emsize = float(std::round(input.size * input.scale * 60.0) / 60.0);
  // A glyph mask is rasterised at the device size, which Skia takes from the
  // vertical extent of the text matrix.
  const float scale = std::sqrt(matrix[2] * matrix[2] + matrix[3] * matrix[3]);
  if (!std::isfinite(emsize) || emsize <= 0 || emsize > 16384 || !(scale > 0)) return false;
  const float linear[4] = {matrix[0] / scale, matrix[1] / scale, matrix[2] / scale,
                           matrix[3] / scale};
  int pen = 0;
  for (size_t start = 0; start < text.size();) {
    size_t end = start;
    SelectedFont font;
    const uint32_t first = Codepoint(text, &end);
    if (!collection_->Select(input, first, &font)) return false;
    while (end < text.size()) {
      size_t next = end;
      const uint32_t code = Codepoint(text, &next);
      if (!Continuation(code)) {
        SelectedFont candidate;
        if (!collection_->Select(input, code, &candidate)) return false;
        if (font.face.Get() != candidate.face.Get() ||
            font.syntheticItalic != candidate.syntheticItalic) break;
      }
      end = next;
    }
    FontInput local = input;
    local.x = local.y = 0;
    FontRun run;
    if (!ShapeFont(font, local, text.substr(start, end - start), &run)) return false;
    int glyph_pen = pen;
    for (size_t i = 0; i < run.glyphs.size(); ++i) {
      // gfxDWriteFont::ShouldRoundXOffset is false for the natural measuring
      // mode Canvas text uses, so the shaper keeps gfxHarfBuzzShaper's plain
      // rounding to app units: no residual carry, one rounded advance per glyph.
      const int offset = int(std::floor(run.glyph_offsets_x[i] * 60.0f + 0.5f));
      const int advance = int(std::floor(run.glyph_advances[i] * 60.0f + 0.5f));
      const double x = input.x + double(glyph_pen + offset) / 60.0;
      const double y = input.y + double(std::floor(run.glyph_offsets_y[i] * 60.0f + 0.5f)) / 60.0;
      const float device_x = float(matrix[0] * x + matrix[2] * y + matrix[4]);
      const float device_y = float(matrix[1] * x + matrix[3] * y + matrix[5]);
      CanvasGlyphMask mask;
      // gfxFont::RenderColorGlyph: up to an adjusted size of 256 a COLR glyph is
      // a cached 2x snapshot the canvas resamples at the rounded glyph origin;
      // an upright run is all the snapshot path models, so a rotated or skewed
      // run (and a larger glyph) fills its layers directly as subpixel masks.
      const bool upright = linear[1] == 0.0f && linear[2] == 0.0f;
      const bool sampled = emsize <= 256.0f && upright;
      int color_x = 0, color_y = 0;
      float color_sx = 0, color_sy = 0;
      SnapPosition(device_x, &color_x, &color_sx);
      SnapPosition(device_y, &color_y, &color_sy);
      if (GlyphColorMask(font, sampled ? emsize : emsize * scale, sampled, linear, color_sx,
                         color_sy, run.glyphs[i], rgb, &mask)) {
        if (sampled) {
          mask.origin_x = x;
          mask.origin_y = y;
        } else {
          for (auto& layer : mask.layers) {
            layer.left += color_x;
            layer.top += color_y;
          }
        }
        if (!mask.layers.empty()) result->push_back(std::move(mask));
      } else {
        int base_x = 0, base_y = 0;
        float sub_x = 0, sub_y = 0;
        SnapPosition(device_x, &base_x, &sub_x);
        SnapPosition(device_y, &base_y, &sub_y);
        if (!GlyphMask(font, emsize * scale, linear, sub_x, sub_y, run.glyphs[i], rgb, &mask))
          return false;
        if (!mask.coverage.empty()) {
          mask.left += base_x;
          mask.top += base_y;
          result->push_back(std::move(mask));
        }
      }
      glyph_pen += advance;
    }
    pen += run.advance;
    start = end;
  }
  return true;
}
}  // namespace ruyijs::web_geometry
