// A COLR glyph reaches the canvas as palette layers (web_geometry). Firefox
// fills them OP_OVER with alpha 1 into a 2x B8G8R8A8 snapshot, then
// DrawTargetSkia::DrawSurface draws that snapshot into the glyph box with
// SkFilterMode::kLinear. A premultiplied N32 image under a scale+translate
// matrix takes Skia's legacy shader (SkBitmapProcState bilerp with 4-bit
// weights, then the paint-alpha scale) and S32A_Opaque_BlitRow32. A fill colour
// that is neither opaque nor transparent puts the run in a group first, which
// is blended back with that alpha (SkARGB32 Blit32 = BlitSprite).
#include "web_canvas/color_glyph.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "web_canvas/raster_blitter.h"

namespace ruyijs::web_canvas::raster {
namespace {
Paint LayerPaint(const uint8_t rgba[4], unsigned alpha_scale) {
  // SkPreMultiplyARGB of the pattern colour with the draw alpha folded in.
  Paint paint;
  const unsigned a = alpha_scale >= 256 ? rgba[3] : (unsigned(rgba[3]) * alpha_scale) >> 8;
  for (int i = 0; i < 3; ++i) paint.rgba[i] = uint8_t((unsigned(rgba[i]) * a + 127) / 255);
  paint.rgba[3] = uint8_t(a);
  return paint;
}
// SkBitmapProcState pack(): clamp-tiled integer pair and the 4-bit lerp weight.
void Pack(int64_t f, int max, int* i0, int* i1, int* w) {
  *i0 = int(std::clamp<int64_t>(f >> 16, 0, max));
  *i1 = int(std::clamp<int64_t>((f + 65536) >> 16, 0, max));
  *w = int((f >> 12) & 0xf);
}
}  // namespace

void DrawColorGlyph(Surface surface, const web_geometry::CanvasGlyphMask& mask,
                    const float matrix[6], uint8_t global, uint8_t group) {
  if (mask.layers.empty() || global == 0) return;
  Paint blend;
  blend.rgba[3] = group;
  if (!mask.sampled) {
    int left = mask.layers[0].left, top = mask.layers[0].top, right = left, bottom = top;
    for (const auto& layer : mask.layers) {
      left = std::min(left, layer.left);
      top = std::min(top, layer.top);
      right = std::max(right, layer.left + layer.width);
      bottom = std::max(bottom, layer.top + layer.height);
    }
    if (right <= left || bottom <= top) return;
    std::vector<uint8_t> sprite(size_t(right - left) * size_t(bottom - top) * 4u, 0);
    Surface target{sprite.data(), right - left, bottom - top};
    for (const auto& layer : mask.layers)
      BlitMask(target, LayerPaint(layer.rgba, unsigned(global) + 1), layer.left - left,
               layer.top - top, layer.coverage.data(), layer.width, layer.height);
    BlitSprite(surface, blend, left, top, sprite.data(), right - left, bottom - top);
    return;
  }
  if (matrix[0] <= 0 || matrix[3] <= 0 || matrix[1] != 0 || matrix[2] != 0) return;
  const int sw = mask.width * 2, sh = mask.height * 2;
  std::vector<uint8_t> snapshot(size_t(sw) * size_t(sh) * 4u, 0);
  Surface shot{snapshot.data(), sw, sh};
  for (const auto& layer : mask.layers)
    BlitMask(shot, LayerPaint(layer.rgba, 256), layer.left, layer.top, layer.coverage.data(),
             layer.width, layer.height);
  // Rect(roundf(point) + bounds.TopLeft(), bounds.Size()) through the transform.
  const double ux = double(std::round(float(mask.origin_x))) + mask.left;
  const double uy = double(std::round(float(mask.origin_y))) + mask.top;
  const double dx = matrix[0] * ux + matrix[4], dy = matrix[3] * uy + matrix[5];
  const double dw = matrix[0] * mask.width, dh = matrix[3] * mask.height;
  const int x0 = int(std::floor(dx)), y0 = int(std::floor(dy));
  const int w = int(std::ceil(dx + dw)) - x0, h = int(std::ceil(dy + dh)) - y0;
  if (w <= 0 || h <= 0) return;
  std::vector<uint8_t> sprite(size_t(w) * size_t(h) * 4u, 0);
  const double sx = double(sw) / dw, sy = double(sh) / dh;
  const unsigned scale = unsigned(global) + 1;
  for (int y = 0; y < h; ++y) {
    const double cy = y0 + y + 0.5;
    if (cy < dy || cy >= dy + dh) continue;
    int r0, r1, wy;
    Pack(int64_t((cy - dy) * sy * 65536.0) - 32768, sh - 1, &r0, &r1, &wy);
    for (int x = 0; x < w; ++x) {
      const double cx = x0 + x + 0.5;
      if (cx < dx || cx >= dx + dw) continue;
      int c0, c1, wx;
      Pack(int64_t((cx - dx) * sx * 65536.0) - 32768, sw - 1, &c0, &c1, &wx);
      const uint8_t* a = &snapshot[(size_t(r0) * sw + c0) * 4u];
      const uint8_t* b = &snapshot[(size_t(r0) * sw + c1) * 4u];
      const uint8_t* c = &snapshot[(size_t(r1) * sw + c0) * 4u];
      const uint8_t* d = &snapshot[(size_t(r1) * sw + c1) * 4u];
      uint8_t* out = &sprite[(size_t(y) * w + x) * 4u];
      for (int i = 0; i < 4; ++i) {
        const unsigned top = a[i] * (16u - wx) + b[i] * unsigned(wx);
        const unsigned bot = c[i] * (16u - wx) + d[i] * unsigned(wx);
        unsigned px = (top * (16u - wy) + bot * unsigned(wy)) >> 8;
        if (scale < 256) px = (px * scale) >> 8;
        out[i] = uint8_t(px);
      }
    }
  }
  BlitSprite(surface, blend, x0, y0, sprite.data(), w, h);
}
}  // namespace ruyijs::web_canvas::raster
