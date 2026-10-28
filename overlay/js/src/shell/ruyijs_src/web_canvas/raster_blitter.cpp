// SkRasterPipelineBlitter's blit entry points, reduced to per-pixel alphas: the
// run encoding Skia keeps is only a compact form of the same row.
#include "web_canvas/raster_blitter.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace ruyijs::web_canvas::raster {
namespace {
inline uint8_t Div255(uint32_t value) { return uint8_t((value + 255) / 256); }
// flush() snaps alphas close to opaque or clear, as Skia's RLE blitter does.
inline uint8_t SnapAlpha(uint8_t alpha) { return alpha > 247 ? 0xff : alpha < 8 ? 0x00 : alpha; }
}  // namespace

// SkBlendMode_ShouldPreScaleCoverage: the coverage scales the source for these
// modes, and lerps the blended result for every other mode.
bool Blitter::PreScaleCoverage(Blend blend) {
  switch (blend) {
    case Blend::kDestination: case Blend::kDestinationOver: case Blend::kPlus:
    case Blend::kDestinationOut: case Blend::kSourceATop: case Blend::kSourceOver:
    case Blend::kXor:
      return true;
    default:
      return false;
  }
}

void Blitter::BlendPixel(int x, int y, uint8_t coverage) {
  if (coverage == 0 || x < 0 || y < 0 || x >= surface_.width || y >= surface_.height) return;
  uint8_t* pixel = surface_.pixels + (size_t(y) * size_t(surface_.width) + size_t(x)) * 4u;
  uint8_t out[4];
  if (LegacySourceOver()) {
    // SkAlphaMulQ(fPMColor, SkAlpha255To256(coverage)) then SkBlitRow::Color32.
    const unsigned scale = unsigned(coverage) + 1;
    uint8_t src[4];
    for (int i = 0; i < 4; ++i) src[i] = uint8_t((unsigned(paint_.rgba[i]) * scale) >> 8);
    const unsigned invAlpha = 256u - src[3];
    for (int i = 0; i < 4; ++i) {
      out[i] = uint8_t(((unsigned(pixel[i]) * invAlpha) >> 8) + src[i]);
    }
  } else if (PreScaleCoverage(paint_.blend)) {
    uint8_t src[4];
    for (int i = 0; i < 4; ++i) src[i] = Div255(unsigned(paint_.rgba[i]) * coverage);
    for (int i = 0; i < 3; ++i) out[i] = Mix(src[i], pixel[i], src[3], pixel[3]);
    out[3] = MixAlpha(src[3], pixel[3]);
  } else {
    uint8_t blended[4];
    for (int i = 0; i < 3; ++i) {
      blended[i] = Mix(paint_.rgba[i], pixel[i], paint_.rgba[3], pixel[3]);
    }
    blended[3] = MixAlpha(paint_.rgba[3], pixel[3]);
    const unsigned inverse = 255u - coverage;
    for (int i = 0; i < 4; ++i) {
      out[i] = Div255(unsigned(pixel[i]) * inverse + unsigned(blended[i]) * coverage);
    }
  }
  std::memcpy(pixel, out, 4);
}

void Blitter::BlitH(int x, int y, int width) {
  for (int i = 0; i < width; ++i) BlendPixel(x + i, y, 0xff);
}

namespace {
// SkFastFourByteInterp: one 8-bit lerp per channel with scale = weight + weight/128.
void FastInterpPixel(uint8_t* pixel, const uint8_t* color, uint8_t weight) {
  const unsigned scale = unsigned(weight) + (unsigned(weight) >> 7);
  const unsigned inverse = 256u - scale;
  for (int i = 0; i < 4; ++i) {
    pixel[i] = uint8_t((unsigned(color[i]) * scale + unsigned(pixel[i]) * inverse) >> 8);
  }
}
}  // namespace

void Blitter::BlitAntiH2(int x, int y, uint8_t a0, uint8_t a1) {
  if (OpaqueBlack() && !wrapped_) {
    const uint8_t weights[2] = {a0, a1};
    for (int i = 0; i < 2; ++i) {
      const int px = x + i;
      if (px < 0 || y < 0 || px >= surface_.width || y >= surface_.height) continue;
      uint8_t* pixel = surface_.pixels + (size_t(y) * size_t(surface_.width) + size_t(px)) * 4u;
      const unsigned scale = 256u - weights[i];
      for (int c = 0; c < 4; ++c) pixel[c] = uint8_t((unsigned(pixel[c]) * scale) >> 8);
      pixel[3] = uint8_t(pixel[3] + weights[i]);
    }
    return;
  }
  // SkRectClipBlitter/SkRgnClipBlitter do not override blitAntiH2, so a path
  // that is clipped horizontally reaches the run blitter instead of the
  // opaque blitter's fast interpolator.
  if (OpaqueSourceOver() && !wrapped_) {
    const uint8_t weights[2] = {a0, a1};
    for (int i = 0; i < 2; ++i) {
      const int px = x + i;
      if (px < 0 || y < 0 || px >= surface_.width || y >= surface_.height) continue;
      uint8_t* pixel = surface_.pixels + (size_t(y) * size_t(surface_.width) + size_t(px)) * 4u;
      FastInterpPixel(pixel, paint_.rgba, weights[i]);
    }
    return;
  }
  BlendPixel(x, y, a0);
  BlendPixel(x + 1, y, a1);
}

void Blitter::BlitAntiRect(int x, int y, int width, int height, uint8_t leftAlpha,
                           uint8_t rightAlpha) {
  if (leftAlpha > 0) BlitV(x, y, height, leftAlpha);
  ++x;
  if (width > 0) {
    for (int row = 0; row < height; ++row) BlitH(x, y + row, width);
    x += width;
  }
  if (rightAlpha > 0) BlitV(x, y, height, rightAlpha);
}

void Blitter::BlitV(int x, int y, int height, uint8_t alpha) {
  for (int i = 0; i < height; ++i) BlendPixel(x, y + i, alpha);
}

void Blitter::BlitAlphas(int x, int y, const uint8_t* alphas, int count) {
  for (int i = 0; i < count; ++i) {
    if (alphas[i] != 0) BlendPixel(x + i, y, alphas[i]);
  }
}
}  // namespace ruyijs::web_canvas::raster
