// SkScan_AAAPath.cpp's trapezoid row: the exact area of a trapezoid inside each
// pixel of one scanline, as 8-bit coverage. The excluded corner triangles are
// computed directly, exactly as the source does (including its coarse
// partial_triangle_to_alpha approximation, which the pixel values depend on).

#include "web_canvas/raster_trapezoid.h"

#include <vector>

namespace ruyijs::web_canvas::raster {

namespace {

uint8_t GetPartialAlpha(uint8_t alpha, Fixed partialHeight) {
  return uint8_t((alpha * partialHeight + kFixedHalf) >> 16);
}

uint8_t GetPartialAlpha(uint8_t alpha, uint8_t fullAlpha) {
  return uint8_t((alpha * fullAlpha) >> 8);
}

uint8_t TrapezoidToAlpha(Fixed l1, Fixed l2) { return uint8_t(((l1 + l2) / 2) >> 8); }

uint8_t PartialTriangleToAlpha(Fixed a, Fixed b) {
  const int64_t area = int64_t(a >> 11) * (a >> 11) * (b >> 11);
  return uint8_t((area >> 8) & 0xff);
}

// The coverage of the part of a line segment above the pixel row.
void ComputeAlphaAboveLine(uint8_t* alphas, Fixed l, Fixed r, Fixed dY, uint8_t fullAlpha) {
  const int count = CeilToInt(r);
  if (count == 0) return;
  if (count == 1) {
    alphas[0] = GetPartialAlpha(uint8_t(((count << 17) - l - r) >> 9), fullAlpha);
    return;
  }
  const Fixed first = kFixed1 - l;
  const Fixed last = r - Fixed((count - 1) << 16);
  const Fixed firstH = FixedMul(first, dY);
  alphas[0] = uint8_t(FixedMul(first, firstH) >> 9);
  Fixed alpha16 = SatAdd(firstH, dY >> 1);
  for (int i = 1; i < count - 1; ++i) {
    alphas[i] = uint8_t(alpha16 >> 8);
    alpha16 = SatAdd(alpha16, dY);
  }
  alphas[count - 1] = uint8_t(fullAlpha - PartialTriangleToAlpha(last, dY));
}

void ComputeAlphaBelowLine(uint8_t* alphas, Fixed l, Fixed r, Fixed dY, uint8_t fullAlpha) {
  const int count = CeilToInt(r);
  if (count == 0) return;
  if (count == 1) {
    alphas[0] = GetPartialAlpha(TrapezoidToAlpha(l, r), fullAlpha);
    return;
  }
  const Fixed first = kFixed1 - l;
  const Fixed last = r - Fixed((count - 1) << 16);
  const Fixed lastH = FixedMul(last, dY);
  alphas[count - 1] = uint8_t(FixedMul(last, lastH) >> 9);
  Fixed alpha16 = SatAdd(lastH, dY >> 1);
  for (int i = count - 2; i > 0; --i) {
    alphas[i] = uint8_t((alpha16 >> 8) & 0xff);
    alpha16 = SatAdd(alpha16, dY);
  }
  alphas[0] = uint8_t(fullAlpha - PartialTriangleToAlpha(first, dY));
}
}  // namespace

// blit_aaa_trapezoid_row: one row of at most 31+ pixels, so the alpha buffer is
// allocated once instead of through the mask blitter.
void BlitAaaTrapezoidRow(AdditiveBlitter& blitter, int y, Fixed ul, Fixed ur, Fixed ll, Fixed lr,
                         Fixed leftDy, Fixed rightDy, uint8_t fullAlpha, bool noRealBlitter) {
  const int left = FloorToInt(ul);
  const int length = CeilToInt(lr) - left;
  // A crossed or degenerate row covers nothing; Skia relies on the walkers
  // never producing one, and the canvas area limit keeps the buffers small.
  if (ul > ur || ll > lr || length <= 0) return;
  if (length == 1) {
    const uint8_t alpha = TrapezoidToAlpha(ur - ul, lr - ll);
    if (fullAlpha == 0xff && !noRealBlitter) blitter.BlitRow(left, y, &alpha, 1);
    else blitter.BlitAntiH(left, y, GetPartialAlpha(alpha, fullAlpha));
    return;
  }
  std::vector<uint8_t> alphas(size_t(length) + 1, fullAlpha);
  std::vector<uint8_t> temp(size_t(length) + 3, 0);
  uint8_t* vtemp = temp.data() + 1;
  const int upperLeft = FloorToInt(ul);
  const int lowerLeft = CeilToInt(ll);
  if (upperLeft + 2 == lowerLeft) {
    const Fixed first = IntToFixed(upperLeft) + kFixed1 - ul;
    const Fixed second = ll - ul - first;
    const uint8_t a1 = uint8_t(fullAlpha - PartialTriangleToAlpha(first, leftDy));
    const uint8_t a2 = PartialTriangleToAlpha(second, leftDy);
    alphas[0] = alphas[0] > a1 ? uint8_t(alphas[0] - a1) : 0;
    alphas[1] = alphas[1] > a2 ? uint8_t(alphas[1] - a2) : 0;
  } else {
    ComputeAlphaBelowLine(vtemp + upperLeft - left, ul - IntToFixed(upperLeft),
                          ll - IntToFixed(upperLeft), leftDy, fullAlpha);
    for (int i = upperLeft; i < lowerLeft; ++i) {
      if (alphas[size_t(i - left)] > vtemp[i - left]) alphas[size_t(i - left)] -= vtemp[i - left];
      else alphas[size_t(i - left)] = 0;
    }
  }
  const int upperRite = FloorToInt(ur);
  const int lowerRite = CeilToInt(lr);
  if (upperRite + 2 == lowerRite) {
    const Fixed first = IntToFixed(upperRite) + kFixed1 - ur;
    const Fixed second = lr - ur - first;
    const uint8_t a1 = PartialTriangleToAlpha(first, rightDy);
    const uint8_t a2 = uint8_t(fullAlpha - PartialTriangleToAlpha(second, rightDy));
    alphas[size_t(length) - 2] = alphas[size_t(length) - 2] > a1
                                     ? uint8_t(alphas[size_t(length) - 2] - a1) : 0;
    alphas[size_t(length) - 1] = alphas[size_t(length) - 1] > a2
                                     ? uint8_t(alphas[size_t(length) - 1] - a2) : 0;
  } else {
    ComputeAlphaAboveLine(vtemp + upperRite - left, ur - IntToFixed(upperRite),
                          lr - IntToFixed(upperRite), rightDy, fullAlpha);
    for (int i = upperRite; i < lowerRite; ++i) {
      if (alphas[size_t(i - left)] > vtemp[i - left]) alphas[size_t(i - left)] -= vtemp[i - left];
      else alphas[size_t(i - left)] = 0;
    }
  }
  if (fullAlpha == 0xff && !noRealBlitter) {
    blitter.BlitRow(left, y, alphas.data(), length);
  } else {
    blitter.BlitAntiH(left, y, alphas.data(), length);
  }
}
}  // namespace ruyijs::web_canvas::raster
