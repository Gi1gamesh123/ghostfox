// SkEdgeClipper.cpp's shared chopping helpers: the clip stages both cut a
// monotone curve at a clip boundary before splitting it in X.
#include "web_canvas/raster_clip.h"

namespace ruyijs::web_canvas::raster {
void ClampLe(float* value, float max) {
  if (*value > max) *value = max;
}
void ClampGe(float* value, float min) {
  if (*value < min) *value = min;
}

bool SortIncreasingY(Point dst[], const Point src[], int count) {
  if (src[0].y > src[count - 1].y) {
    for (int i = 0; i < count; ++i) dst[i] = src[count - i - 1];
    return true;
  }
  for (int i = 0; i < count; ++i) dst[i] = src[i];
  return false;
}

// SkBezierCubic's axis intersection: Skia solves the cubic and checks the
// residual; bisecting the (monotone) coordinate to double precision reaches the
// same float control points.
float BisectAt(float c0, float c1, float c2, float c3, float target) {
  const auto eval = [c0, c1, c2, c3](double t) {
    const double mt = 1 - t;
    return mt * mt * mt * c0 + 3 * mt * mt * t * c1 + 3 * mt * t * t * c2 + t * t * t * c3;
  };
  double lo = 0, hi = 1;
  const bool increasing = c3 > c0;
  for (int i = 0; i < 90; ++i) {
    const double mid = (lo + hi) / 2;
    const double value = eval(mid);
    if ((value < target) == increasing) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  return float((lo + hi) / 2);
}

float BisectQuadAt(float c0, float c1, float c2, float target) {
  const auto eval = [c0, c1, c2](double t) {
    const double mt = 1 - t;
    return mt * mt * c0 + 2 * mt * t * c1 + t * t * c2;
  };
  double lo = 0, hi = 1;
  const bool increasing = c2 > c0;
  for (int i = 0; i < 90; ++i) {
    const double mid = (lo + hi) / 2;
    const double value = eval(mid);
    if ((value < target) == increasing) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  return float((lo + hi) / 2);
}

void ChopMonoCubicAtY(Point src[4], float y, Point dst[7]) {
  ChopCubicAt(src, dst, BisectAt(src[0].y, src[1].y, src[2].y, src[3].y, y));
}

void ChopMonoCubicAtX(Point src[4], float x, Point dst[7]) {
  ChopCubicAt(src, dst, BisectAt(src[0].x, src[1].x, src[2].x, src[3].x, x));
}

// chop_cubic_in_Y: modify pts[] so that it is clipped in Y to the clip rect.
void ChopCubicInY(Point pts[4], const ClipRect& clip) {
  if (pts[0].y < clip.top) {
    Point tmp[7];
    ChopMonoCubicAtY(pts, clip.top, tmp);
    if (tmp[3].y < clip.top && tmp[4].y < clip.top && tmp[5].y < clip.top) {
      Point tmp2[4] = {tmp[3], tmp[4], tmp[5], tmp[6]};
      ChopMonoCubicAtY(tmp2, clip.top, tmp);
    }
    tmp[3].y = clip.top;
    ClampGe(&tmp[4].y, clip.top);
    pts[0] = tmp[3];
    pts[1] = tmp[4];
    pts[2] = tmp[5];
  }
  if (pts[3].y > clip.bottom) {
    Point tmp[7];
    ChopMonoCubicAtY(pts, clip.bottom, tmp);
    tmp[3].y = clip.bottom;
    ClampLe(&tmp[2].y, clip.bottom);
    pts[1] = tmp[1];
    pts[2] = tmp[2];
    pts[3] = tmp[3];
  }
}

// chop_quad_in_Y: the same for a y-monotone quad.
void ChopQuadInY(Point pts[3], const ClipRect& clip) {
  Point tmp[5];
  if (pts[0].y < clip.top) {
    ChopQuadAt(pts, tmp, BisectQuadAt(pts[0].y, pts[1].y, pts[2].y, clip.top));
    tmp[2].y = clip.top;
    ClampGe(&tmp[3].y, clip.top);
    pts[0] = tmp[2];
    pts[1] = tmp[3];
  }
  if (pts[2].y > clip.bottom) {
    ChopQuadAt(pts, tmp, BisectQuadAt(pts[0].y, pts[1].y, pts[2].y, clip.bottom));
    ClampLe(&tmp[1].y, clip.bottom);
    tmp[2].y = clip.bottom;
    pts[1] = tmp[1];
    pts[2] = tmp[2];
  }
}
}  // namespace ruyijs::web_canvas::raster
