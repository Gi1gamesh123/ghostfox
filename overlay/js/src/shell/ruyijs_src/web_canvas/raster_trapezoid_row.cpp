// blit_trapezoid_row: splits one trapezoid row into the two corner triangles,
// the full-coverage rectangle between them and two inner trapezoids, exactly as
// SkScan_AAAPath.cpp does before handing the pieces to blit_aaa_trapezoid_row.
#include "web_canvas/raster_trapezoid.h"

namespace ruyijs::web_canvas::raster {
namespace {
uint8_t GetPartialAlpha(uint8_t alpha, uint8_t fullAlpha) {
  return uint8_t((alpha * fullAlpha) >> 8);
}
uint8_t TrapezoidToAlpha(Fixed l1, Fixed l2) { return uint8_t(((l1 + l2) / 2) >> 8); }
uint8_t PartialTriangleToAlpha(Fixed a, Fixed b) {
  const int64_t area = int64_t(a >> 11) * (a >> 11) * (b >> 11);
  return uint8_t((area >> 8) & 0xff);
}
Fixed ApproximateIntersection(Fixed l1, Fixed r1, Fixed l2, Fixed r2) {
  if (l1 > r1) { const Fixed s = l1; l1 = r1; r1 = s; }
  if (l2 > r2) { const Fixed s = l2; l2 = r2; r2 = s; }
  return ((l1 > l2 ? l1 : l2) + (r1 < r2 ? r1 : r2)) / 2;
}
}  // namespace

void BlitTrapezoid(AdditiveBlitter& blitter, int y, Fixed ul, Fixed ur, Fixed ll, Fixed lr,
                   Fixed leftDy, Fixed rightDy, uint8_t fullAlpha, bool noRealBlitter) {
  if (ul > ur) return;
  if (ll > lr) ll = lr = ApproximateIntersection(ul, ll, ur, lr);
  if (ul == ur && ll == lr) return;
  if (ul > ll) { const Fixed s = ul; ul = ll; ll = s; }
  if (ur > lr) { const Fixed s = ur; ur = lr; lr = s; }
  const Fixed joinLeft = CeilToFixed(ll);
  const Fixed joinRite = FloorToFixed(ur);
  if (joinLeft > joinRite) {
    BlitAaaTrapezoidRow(blitter, y, ul, ur, ll, lr, leftDy, rightDy, fullAlpha, noRealBlitter);
    return;
  }
  if (ul < joinLeft) {
    const int length = CeilToInt(joinLeft - ul);
    if (length == 1) {
      const uint8_t alpha = TrapezoidToAlpha(joinLeft - ul, joinLeft - ll);
      if (fullAlpha == 0xff && !noRealBlitter) blitter.BlitRow(ul >> 16, y, &alpha, 1);
      else blitter.BlitAntiH(ul >> 16, y, GetPartialAlpha(alpha, fullAlpha));
    } else if (length == 2) {
      const Fixed first = joinLeft - kFixed1 - ul;
      const Fixed second = ll - ul - first;
      const uint8_t a1 = PartialTriangleToAlpha(first, leftDy);
      const uint8_t a2 = uint8_t(fullAlpha - PartialTriangleToAlpha(second, leftDy));
      // No scaling by fullAlpha here: Skia's blit_two_alphas hands a1/a2 over
      // as they are, and a2 already carries fullAlpha.
      if (fullAlpha == 0xff && !noRealBlitter) {
        blitter.BlitPair(ul >> 16, y, a1, a2);
      } else {
        blitter.BlitAntiH(ul >> 16, y, a1);
        blitter.BlitAntiH((ul >> 16) + 1, y, a2);
      }
    } else {
      BlitAaaTrapezoidRow(blitter, y, ul, joinLeft, ll, joinLeft, leftDy, kMaxS32, fullAlpha,
                          noRealBlitter);
    }
  }
  if (joinLeft < joinRite) {
    const int x = FloorToInt(joinLeft);
    const int count = FloorToInt(joinRite - joinLeft);
    if (fullAlpha == 0xff && !noRealBlitter) blitter.BlitRowH(x, y, count);
    else blitter.BlitAntiH(x, y, count, fullAlpha);
  }
  if (lr > joinRite) {
    const int length = CeilToInt(lr - joinRite);
    if (length == 1) {
      const uint8_t alpha = TrapezoidToAlpha(ur - joinRite, lr - joinRite);
      if (fullAlpha == 0xff && !noRealBlitter) blitter.BlitRow(joinRite >> 16, y, &alpha, 1);
      else blitter.BlitAntiH(joinRite >> 16, y, uint8_t((alpha * fullAlpha) >> 8));
    } else if (length == 2) {
      const Fixed first = joinRite + kFixed1 - ur;
      const Fixed second = lr - ur - first;
      const uint8_t a1 = uint8_t(fullAlpha - PartialTriangleToAlpha(first, rightDy));
      const uint8_t a2 = PartialTriangleToAlpha(second, rightDy);
      if (fullAlpha == 0xff && !noRealBlitter) {
        blitter.BlitPair(joinRite >> 16, y, a1, a2);
      } else {
        blitter.BlitAntiH(joinRite >> 16, y, a1);
        blitter.BlitAntiH((joinRite >> 16) + 1, y, a2);
      }
    } else {
      BlitAaaTrapezoidRow(blitter, y, joinRite, ur, joinRite, lr, kMaxS32, rightDy, fullAlpha,
                          noRealBlitter);
    }
  }
}
}  // namespace ruyijs::web_canvas::raster
