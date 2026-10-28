// SkRasterPipeline's lowp blend stages: a coverage byte scales the premultiplied
// source, then the selected blend formula runs in 8-bit integer maths. div255 is
// the approximate form ((v + 255) >> 8) that Skia takes outside NEON, and
// div255_accurate is the exact one the Porter-Duff stages use.
#include "web_canvas/raster_blitter.h"

namespace ruyijs::web_canvas::raster {
namespace {
inline uint8_t Div255(uint32_t value) { return uint8_t((value + 255) / 256); }
inline uint8_t Div255Accurate(uint32_t value) {
  value += 128;
  return uint8_t((value + value / 256) / 256);
}
inline uint8_t Inv(uint8_t value) { return uint8_t(255 - value); }
inline uint32_t Max(uint32_t a, uint32_t b) { return a > b ? a : b; }
inline uint32_t Min(uint32_t a, uint32_t b) { return a < b ? a : b; }
inline uint8_t Clamp(int value) { return uint8_t(value > 255 ? 255 : value); }
}  // namespace

uint8_t Blitter::Mix(uint8_t src, uint8_t dst, uint8_t srcAlpha, uint8_t dstAlpha) const {
  switch (paint_.blend) {
    case Blend::kSourceOver: return Clamp(src + Div255Accurate(dst * Inv(srcAlpha)));
    case Blend::kDestinationOver: return Clamp(dst + Div255Accurate(src * Inv(dstAlpha)));
    case Blend::kSourceIn: return Div255Accurate(src * dstAlpha);
    case Blend::kDestinationIn: return Div255Accurate(dst * srcAlpha);
    case Blend::kSourceOut: return Div255Accurate(src * Inv(dstAlpha));
    case Blend::kDestinationOut: return Div255Accurate(dst * Inv(srcAlpha));
    case Blend::kSourceATop: return Div255(src * dstAlpha + dst * Inv(srcAlpha));
    case Blend::kDestinationATop: return Div255(dst * srcAlpha + src * Inv(dstAlpha));
    case Blend::kXor: return Div255(src * Inv(dstAlpha) + dst * Inv(srcAlpha));
    case Blend::kPlus: return Clamp(int(src) + dst);
    case Blend::kCopy: return src;
    case Blend::kDestination: return dst;
    case Blend::kMultiply:
      return Div255(src * Inv(dstAlpha) + dst * Inv(srcAlpha) + src * dst);
    case Blend::kScreen: return Clamp(int(src) + dst - Div255Accurate(src * dst));
    case Blend::kDifference:
      return Clamp(int(src) + dst - 2 * Div255(Min(src * dstAlpha, dst * srcAlpha)));
    case Blend::kDarken: return Clamp(int(src) + dst - Div255(Max(src * dstAlpha,
                                                                 dst * srcAlpha)));
    case Blend::kLighten: return Clamp(int(src) + dst - Div255(Min(src * dstAlpha,
                                                                  dst * srcAlpha)));
    case Blend::kExclusion: return Clamp(int(src) + dst - 2 * Div255(uint32_t(src) * dst));
    case Blend::kOverlay:
    case Blend::kHardLight: {
      const bool first = paint_.blend == Blend::kOverlay ? 2 * dst <= dstAlpha
                                                         : 2 * src <= srcAlpha;
      const uint32_t product = first ? 2u * src * dst
                                     : uint32_t(srcAlpha) * dstAlpha -
                                           2u * (srcAlpha - src) * (dstAlpha - dst);
      return Div255(src * Inv(dstAlpha) + dst * Inv(srcAlpha) + product);
    }
  }
  return Div255Accurate(dst * Inv(srcAlpha));
}

// The separable blend group (darken..overlay) keeps source-over alpha.
uint8_t Blitter::MixAlpha(uint8_t srcAlpha, uint8_t dstAlpha) const {
  switch (paint_.blend) {
    case Blend::kDarken:
    case Blend::kLighten:
    case Blend::kDifference:
    case Blend::kExclusion:
    case Blend::kOverlay:
    case Blend::kHardLight:
      return Clamp(srcAlpha + Div255(dstAlpha * Inv(srcAlpha)));
    default:
      return Mix(srcAlpha, dstAlpha, srcAlpha, dstAlpha);
  }
}
}  // namespace ruyijs::web_canvas::raster
