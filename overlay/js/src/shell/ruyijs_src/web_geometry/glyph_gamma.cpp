// SkMaskGamma: the table Skia applies to a grayscale glyph mask before the
// 8-bit blend, so the anti-aliased edge follows the same curve the DWrite
// rendering parameters describe. web_geometry has no Skia, so the luminance
// model and the correcting-table loop are ported as plain floats.
#include <cmath>
#include <cstdint>

#include "web_geometry/glyph_mask.h"

namespace ruyijs::web_geometry {
namespace {
// SkColorSpaceLuminance: gamma 0 is sRGB, 1 is linear, anything else a power.
double ToLuma(double gamma, double value) {
  if (gamma == 0) return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
  if (gamma == 1) return value;
  return std::pow(value, gamma);
}
double FromLuma(double gamma, double luma) {
  if (gamma == 0) {
    return luma <= 0.0031308 ? luma * 12.92 : 1.055 * std::pow(luma, 1 / 2.4) - 0.055;
  }
  if (gamma == 1) return luma;
  return std::pow(luma, 1 / gamma);
}
float ApplyContrast(float srca, float contrast) {
  return srca + ((1.0f - srca) * contrast * srca);
}
// sk_t_scale255<N>: an N-bit channel spread over 0..255.
uint8_t Scale(unsigned value, int bits) {
  const uint8_t base = uint8_t(value << (8 - bits));
  uint8_t spread = base;
  for (int i = bits; i < 8; i += bits) spread |= uint8_t(base >> i);
  return spread;
}
// SkScalerContext::PreprocessRec: an A8 mask keys its table by the luminance of
// the canonical (3,3,2) colour rather than by the green channel, and the table
// itself is built for that luminance's three-bit step.
unsigned TableIndex(const uint8_t rgb[3]) {
  const unsigned r = Scale(rgb[0] >> 5, 3), g = Scale(rgb[1] >> 5, 3);
  const unsigned b = Scale(rgb[2] >> 6, 2);
  // SkComputeLuminance: SK_LUM_COEFF_* in 1.8 fixed point, summing to 256.
  const unsigned luminance = (r * 54 + g * 183 + b * 19) >> 8;
  return luminance >> 5;
}
}  // namespace

void GlyphMaskTable(float gamma, float contrast, const uint8_t rgb[3], uint8_t table[256]) {
  const float src = float(Scale(TableIndex(rgb), 3)) / 255.0f;
  const float linSrc = float(ToLuma(gamma, src));
  // The correction assumes the background is the perceptual inverse of the
  // colour, which is what makes the table a function of one luminance.
  const float dst = 1.0f - src;
  const float linDst = float(ToLuma(gamma, dst));
  const float adjustedContrast = contrast * linDst;
  for (int i = 0; i < 256; ++i) {
    const float rawSrca = float(i) / 255.0f;
    const float srca = ApplyContrast(rawSrca, adjustedContrast);
    float value;
    if (std::fabs(src - dst) < (1.0f / 256.0f)) {
      value = srca;
    } else {
      const float linOut = linSrc * srca + (1.0f - srca) * linDst;
      const float out = float(FromLuma(gamma, linOut));
      // Undo what the 8-bit blend will do to the gamma-corrected coverage.
      value = (out - dst) / (src - dst);
    }
    const float scaled = 255.0f * value;
    table[i] = uint8_t(scaled <= 0 ? 0 : scaled >= 255 ? 255 : int(std::floor(scaled + 0.5f)));
  }
}
}  // namespace ruyijs::web_geometry
