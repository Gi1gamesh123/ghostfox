// SkARGB32_Blitter::blitMask, the entry Skia draws an A8 glyph mask through: a
// flat colour, one coverage byte per pixel, and three variants Skia picks by the
// colour (black, opaque, and the general premultiplied blend). The arithmetic is
// Sk4px::approxMulDiv255, (v*c + v) >> 8, which is what the mask blitter uses
// instead of the fill blitter's SkAlphaMulQ plus Color32.
#include "web_canvas/raster_blitter.h"

namespace ruyijs::web_canvas::raster {
namespace {
uint32_t ApproxMulDiv255(uint32_t value, uint32_t coverage) {
  return (value * coverage + value) >> 8;
}
uint8_t Saturate(uint32_t value) { return uint8_t(value > 255 ? 255 : value); }
bool IsBlack(const Paint& paint) {
  return paint.rgba[3] == 0xff && paint.rgba[0] == 0 && paint.rgba[1] == 0 && paint.rgba[2] == 0;
}
}  // namespace

void BlitSprite(Surface surface, const Paint& paint, int left, int top, const uint8_t* rgba,
                int width, int height) {
  if (!surface.pixels || !rgba || width <= 0 || height <= 0) return;
  // SkARGB32_Blit32: a premultiplied source blended with SkBlendARGB32, whose global
  // scale is the paint's alpha (the fillStyle alpha times globalAlpha).
  const unsigned src_scale = unsigned(paint.rgba[3]) + 1;
  if (src_scale == 1) return;
  for (int y = 0; y < height; ++y) {
    const int row = top + y;
    if (row < 0 || row >= surface.height) continue;
    uint8_t* rowPixels = surface.pixels + size_t(row) * size_t(surface.width) * 4u;
    for (int x = 0; x < width; ++x) {
      const int column = left + x;
      if (column < 0 || column >= surface.width) continue;
      const uint8_t* src = rgba + (size_t(y) * size_t(width) + size_t(x)) * 4u;
      uint8_t* dst = rowPixels + size_t(column) * 4u;
      // SkAlphaMulInv256: the destination scale left by the source's own alpha.
      const unsigned product = 0xFFFF - unsigned(src[3]) * src_scale;
      const unsigned dst_scale = (product + (product >> 8)) >> 8;
      for (int i = 0; i < 4; ++i) {
        const unsigned sum = unsigned(src[i]) * src_scale + unsigned(dst[i]) * dst_scale;
        const unsigned value = sum >> 8;
        dst[i] = uint8_t(value > 255 ? 255 : value);
      }
    }
  }
}

void BlitMask(Surface surface, const Paint& paint, int left, int top, const uint8_t* coverage,
              int width, int height) {
  if (!surface.pixels || !coverage || width <= 0 || height <= 0) return;
  if (paint.blend != Blend::kSourceOver) {
    // Every other mode goes through the raster pipeline in Skia, which is the
    // blend a filled path of the same coverage takes.
    Blitter blitter(surface, paint);
    for (int y = 0; y < height; ++y) {
      const int row = top + y;
      if (row < 0 || row >= surface.height) continue;
      blitter.BlitAlphas(left, row, coverage + size_t(y) * size_t(width), width);
    }
    return;
  }
  const bool black = IsBlack(paint), opaque = paint.rgba[3] == 0xff;
  for (int y = 0; y < height; ++y) {
    const int row = top + y;
    if (row < 0 || row >= surface.height) continue;
    uint8_t* pixel = surface.pixels + size_t(row) * size_t(surface.width) * 4u;
    for (int x = 0; x < width; ++x) {
      const int column = left + x;
      const uint8_t alpha = coverage[size_t(y) * size_t(width) + size_t(x)];
      if (alpha == 0 || column < 0 || column >= surface.width) continue;
      uint8_t* dst = pixel + size_t(column) * 4u;
      const uint32_t inverse = 255u - alpha;
      uint8_t out[4];
      if (black) {
        out[0] = uint8_t(ApproxMulDiv255(dst[0], inverse));
        out[1] = uint8_t(ApproxMulDiv255(dst[1], inverse));
        out[2] = uint8_t(ApproxMulDiv255(dst[2], inverse));
        out[3] = Saturate(alpha + ApproxMulDiv255(dst[3], inverse));
      } else if (opaque) {
        for (int i = 0; i < 4; ++i)
          out[i] = Saturate(ApproxMulDiv255(paint.rgba[i], alpha) +
                            ApproxMulDiv255(dst[i], inverse));
      } else {
        // The general form blends with the source alpha the coverage leaves.
        uint8_t src[4];
        for (int i = 0; i < 4; ++i) src[i] = uint8_t(ApproxMulDiv255(paint.rgba[i], alpha));
        const uint32_t remaining = 255u - src[3];
        for (int i = 0; i < 4; ++i)
          out[i] = Saturate(src[i] + ApproxMulDiv255(dst[i], remaining));
      }
      dst[0] = out[0]; dst[1] = out[1]; dst[2] = out[2]; dst[3] = out[3];
    }
  }
}
}  // namespace ruyijs::web_canvas::raster
