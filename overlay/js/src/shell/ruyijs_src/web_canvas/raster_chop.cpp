// SkGeometry.cpp: the y-extrema split Skia applies to every curve before it
// builds analytic edges, so each edge is y-monotone. Float arithmetic only.
#include <cmath>

#include "web_canvas/raster_edge.h"

namespace ruyijs::web_canvas::raster {
// unchecked_mix(a, b, t) = (b - a) * t + a, which Skia prefers for cubics.
float Mix(float a, float b, float t) { return (b - a) * t + a; }

void ChopQuadAt(const Point src[3], Point dst[5], float t) {
  const float ax = src[0].x + (src[1].x - src[0].x) * t;
  const float ay = src[0].y + (src[1].y - src[0].y) * t;
  const float bx = src[1].x + (src[2].x - src[1].x) * t;
  const float by = src[1].y + (src[2].y - src[1].y) * t;
  dst[0] = src[0];
  dst[1] = Point{ax, ay};
  dst[2] = Point{ax + (bx - ax) * t, ay + (by - ay) * t};
  dst[3] = Point{bx, by};
  dst[4] = src[2];
}

void ChopCubicAt(const Point src[4], Point dst[7], float t) {
  if (t == 1) {
    for (int i = 0; i < 4; ++i) dst[i] = src[i];
    dst[4] = dst[5] = dst[6] = src[3];
    return;
  }
  const float abx = Mix(src[0].x, src[1].x, t), aby = Mix(src[0].y, src[1].y, t);
  const float bcx = Mix(src[1].x, src[2].x, t), bcy = Mix(src[1].y, src[2].y, t);
  const float cdx = Mix(src[2].x, src[3].x, t), cdy = Mix(src[2].y, src[3].y, t);
  const float abcx = Mix(abx, bcx, t), abcy = Mix(aby, bcy, t);
  const float bcdx = Mix(bcx, cdx, t), bcdy = Mix(bcy, cdy, t);
  const float abcdx = Mix(abcx, bcdx, t), abcdy = Mix(abcy, bcdy, t);
  dst[0] = src[0];
  dst[1] = Point{abx, aby};
  dst[2] = Point{abcx, abcy};
  dst[3] = Point{abcdx, abcdy};
  dst[4] = Point{bcdx, bcdy};
  dst[5] = Point{cdx, cdy};
  dst[6] = src[3];
}

namespace {
bool IsNotMonotonic(float a, float b, float c) {
  float ab = a - b;
  float bc = b - c;
  if (ab < 0) bc = -bc;
  return ab == 0 || bc < 0;
}

bool ValidUnitDivide(float numer, float denom, float* ratio) {
  if (numer < 0) {
    numer = -numer;
    denom = -denom;
  }
  if (denom == 0 || numer == 0 || numer >= denom) return false;
  const float r = numer / denom;
  if (std::isnan(r) || r == 0) return false;
  *ratio = r;
  return true;
}

int FindUnitQuadRoots(float a, float b, float c, float roots[2]) {
  if (a == 0) return ValidUnitDivide(-c, b, roots) ? 1 : 0;
  float* r = roots;
  double dr = (double)b * b - 4 * (double)a * c;
  if (dr < 0) return 0;
  dr = std::sqrt(dr);
  const float bigR = float(dr);
  if (!std::isfinite(bigR)) return 0;
  const float q = b < 0 ? -(b - bigR) / 2 : -(b + bigR) / 2;
  if (ValidUnitDivide(q, a, r)) ++r;
  if (ValidUnitDivide(c, q, r)) ++r;
  if (r - roots == 2) {
    if (roots[0] > roots[1]) {
      const float swap = roots[0]; roots[0] = roots[1]; roots[1] = swap;
    } else if (roots[0] == roots[1]) {
      --r;
    }
  }
  return int(r - roots);
}

int FindCubicExtrema(float a, float b, float c, float d, float tValues[2]) {
  const float a2 = d - a + 3 * (b - c);
  const float b2 = 2 * (a - b - b + c);
  const float c2 = b - a;
  return FindUnitQuadRoots(a2, b2, c2, tValues);
}

void ChopCubicAtTwo(const Point src[4], Point dst[10], float t0, float t1) {
  if (t1 == 1) {
    ChopCubicAt(src, dst, t0);
    dst[7] = dst[8] = dst[9] = src[3];
    return;
  }
  const float ab0x = Mix(src[0].x, src[1].x, t0), ab0y = Mix(src[0].y, src[1].y, t0);
  const float bc0x = Mix(src[1].x, src[2].x, t0), bc0y = Mix(src[1].y, src[2].y, t0);
  const float cd0x = Mix(src[2].x, src[3].x, t0), cd0y = Mix(src[2].y, src[3].y, t0);
  const float abc0x = Mix(ab0x, bc0x, t0), abc0y = Mix(ab0y, bc0y, t0);
  const float bcd0x = Mix(bc0x, cd0x, t0), bcd0y = Mix(bc0y, cd0y, t0);
  const float abcd0x = Mix(abc0x, bcd0x, t0), abcd0y = Mix(abc0y, bcd0y, t0);
  const float ab1x = Mix(src[0].x, src[1].x, t1), ab1y = Mix(src[0].y, src[1].y, t1);
  const float bc1x = Mix(src[1].x, src[2].x, t1), bc1y = Mix(src[1].y, src[2].y, t1);
  const float cd1x = Mix(src[2].x, src[3].x, t1), cd1y = Mix(src[2].y, src[3].y, t1);
  const float abc1x = Mix(ab1x, bc1x, t1), abc1y = Mix(ab1y, bc1y, t1);
  const float bcd1x = Mix(bc1x, cd1x, t1), bcd1y = Mix(bc1y, cd1y, t1);
  const float abcd1x = Mix(abc1x, bcd1x, t1), abcd1y = Mix(abc1y, bcd1y, t1);
  dst[0] = src[0];
  dst[1] = Point{ab0x, ab0y};
  dst[2] = Point{abc0x, abc0y};
  dst[3] = Point{abcd0x, abcd0y};
  dst[4] = Point{Mix(abc0x, bcd0x, t1), Mix(abc0y, bcd0y, t1)};
  dst[5] = Point{Mix(abc1x, bcd1x, t0), Mix(abc1y, bcd1y, t0)};
  dst[6] = Point{abcd1x, abcd1y};
  dst[7] = Point{bcd1x, bcd1y};
  dst[8] = Point{cd1x, cd1y};
  dst[9] = src[3];
}

void FlattenDoubleQuadExtrema(Point dst[5]) {
  const float flat = dst[2].y;
  dst[1].y = flat;
  dst[3].y = flat;
}
// The three points a y-extremum split leaves at the same y (Skia's
// flatten_double_cubic_extrema, indexed from |first|).
void FlattenDoubleCubicExtrema(Point dst[10], int first) {
  const float flat = dst[first + 3].y;
  dst[first + 2].y = flat;
  dst[first + 4].y = flat;
}
}  // namespace

int ChopQuadAtYExtrema(const Point src[3], Point dst[5]) {
  float a = src[0].y, b = src[1].y, c = src[2].y;
  if (IsNotMonotonic(a, b, c)) {
    float t = 0;
    if (ValidUnitDivide(a - b, a - b - b + c, &t)) {
      ChopQuadAt(src, dst, t);
      FlattenDoubleQuadExtrema(dst);
      return 1;
    }
    b = std::fabs(a - b) < std::fabs(b - c) ? a : c;
  }
  dst[0] = Point{src[0].x, a};
  dst[1] = Point{src[1].x, b};
  dst[2] = Point{src[2].x, c};
  return 0;
}

int ChopCubicAtYExtrema(const Point src[4], Point dst[10]) {
  float tValues[2] = {0, 0};
  const int roots = FindCubicExtrema(src[0].y, src[1].y, src[2].y, src[3].y, tValues);
  if (roots == 0) {
    for (int i = 0; i < 4; ++i) dst[i] = src[i];
    return 0;
  }
  if (roots == 1) {
    ChopCubicAt(src, dst, tValues[0]);
  } else {
    ChopCubicAtTwo(src, dst, tValues[0], tValues[1]);
  }
  FlattenDoubleCubicExtrema(dst, 0);
  if (roots == 2) FlattenDoubleCubicExtrema(dst, 3);
  return roots;
}
}  // namespace ruyijs::web_canvas::raster
