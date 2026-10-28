#ifndef RUYIJS_WEB_CANVAS_RASTER_FIXED_H
#define RUYIJS_WEB_CANVAS_RASTER_FIXED_H
// The fixed-point arithmetic of Skia's analytic path rasterizer, which is what
// gfx/2d's DrawTargetSkia runs for an anti-aliased canvas fill. Only the parts
// the scan converter uses are here: 16.16 SkFixed, FDot6 (1/64 px), the 2.22
// reciprocal table lookup and its saturation helpers.
#include <cmath>
#include <cstdint>

namespace ruyijs::web_canvas::raster {
using Fixed = int32_t;
using FDot6 = int32_t;
inline constexpr Fixed kFixed1 = 1 << 16;
inline constexpr Fixed kFixedHalf = 1 << 15;
inline constexpr int32_t kMaxS32 = 0x7fffffff;
inline constexpr int32_t kMinS32 = -0x7fffffff - 1;

inline Fixed FixedMul(Fixed a, Fixed b) { return Fixed((int64_t(a) * b) >> 16); }
// SkFixedDiv: the dividend may exceed 32 bits, so the quotient saturates.
inline Fixed FixedDiv(Fixed numer, Fixed denom) {
  const int64_t value = (int64_t(numer) << 16) / denom;
  return value > kMaxS32 ? kMaxS32 : value < kMinS32 ? kMinS32 : Fixed(value);
}
inline int32_t SatAdd(int32_t a, int32_t b) {
  const int64_t sum = int64_t(a) + b;
  return sum > kMaxS32 ? kMaxS32 : sum < kMinS32 ? kMinS32 : int32_t(sum);
}
inline int32_t SatSub(int32_t a, int32_t b) {
  const int64_t sum = int64_t(a) - b;
  return sum > kMaxS32 ? kMaxS32 : sum < kMinS32 ? kMinS32 : int32_t(sum);
}
// Edges snap their y to a quarter pixel (SkAnalyticEdge::SnapY, accuracy 2).
inline Fixed SnapY(Fixed y) { return Fixed(((uint32_t(y) + (kFixed1 >> 3)) >> 14) << 14); }
inline Fixed CeilToFixed(Fixed x) { return Fixed(uint32_t(x + kFixed1 - 1) & 0xffff0000u); }
inline Fixed FloorToFixed(Fixed x) { return Fixed(uint32_t(x) & 0xffff0000u); }
inline Fixed RoundToFixed(Fixed x) { return Fixed(uint32_t(x + kFixedHalf) & 0xffff0000u); }
inline int CeilToInt(Fixed x) { return (x + kFixed1 - 1) >> 16; }
inline int FloorToInt(Fixed x) { return x >> 16; }
inline int RoundToInt(Fixed x) { return (x + kFixedHalf) >> 16; }
inline Fixed IntToFixed(int n) { return Fixed(uint32_t(n) << 16); }

inline FDot6 FixedToFDot6(Fixed x) { return x >> 10; }
inline Fixed FDot6ToFixed(FDot6 x) { return x << 10; }
inline Fixed FDot6ToFixedDiv2(FDot6 x) { return Fixed(x) << 9; }
inline int FDot6Round(FDot6 x) { return (x + 32) >> 6; }
inline FDot6 IntToFDot6(int x) { return x << 6; }
// SkScalarRoundToFDot6: Firefox's SkUserConfig.h defines
// SK_RASTERIZE_EVEN_ROUNDING, so the analytic edges round the scaled coordinate
// to nearest (ties to even) instead of truncating it.
inline FDot6 ScalarRoundToFDot6(float x, int shift) {
  return FDot6(std::nearbyint(double(x) * double(1 << (6 + shift))));
}

// SkAnalyticEdge's reciprocal: 1/x scaled by 2^22, for 1 <= |x| <= 1024. The
// source spells it as a 1025 entry table, every entry of which is exactly the
// truncated quotient below, so the table is recomputed instead of copied.
inline Fixed QuickInverse(FDot6 x) {
  if (x == 0) return 0;
  return x > 0 ? Fixed(4194304 / x) : Fixed(-(4194304 / -x));
}

inline Fixed FDot6Div(FDot6 a, FDot6 b) {
  if (a >= -32768 && a <= 32767) return Fixed((a << 16) / b);
  return FixedDiv(a, b);
}

// quick_div: the table lookup is only used for small, non-degenerate slopes.
inline Fixed QuickDiv(FDot6 a, FDot6 b) {
  const FDot6 absA = a < 0 ? -a : a;
  const FDot6 absB = b < 0 ? -b : b;
  if (absB >= 8 && absB < 1024 && absA < 131072) {
    return Fixed((int64_t(a) * QuickInverse(b)) >> 6);
  }
  return FDot6Div(a, b);
}
}  // namespace ruyijs::web_canvas::raster
#endif
