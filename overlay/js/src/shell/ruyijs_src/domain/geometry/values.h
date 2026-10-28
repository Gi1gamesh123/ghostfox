#pragma once
#include <algorithm>

namespace ruyijs::domain::geometry {
// x/y/width/height are the unique values; derived sides are never cached.
inline double RectSide(const double v[4], unsigned side) noexcept {
  const double position = v[(side == 0 || side == 2) ? 1 : 0];
  const double extent = v[(side == 0 || side == 2) ? 3 : 2];
  const double other = position + extent;
  return side < 2 ? (side == 0 ? std::min(position, other) : std::max(position, other))
                  : (side == 2 ? std::max(position, other) : std::min(position, other));
}
inline void QuadBounds(const double points[8], double rect[8]) noexcept {
  double left = points[0], right = points[0], top = points[1], bottom = points[1];
  for (unsigned i = 2; i < 8; i += 2) {
    left = std::min(left, points[i]); right = std::max(right, points[i]);
    top = std::min(top, points[i + 1]); bottom = std::max(bottom, points[i + 1]);
  }
  rect[0] = left; rect[1] = top; rect[2] = right - left; rect[3] = bottom - top;
  rect[4] = top; rect[5] = right; rect[6] = bottom; rect[7] = left;
}
}
