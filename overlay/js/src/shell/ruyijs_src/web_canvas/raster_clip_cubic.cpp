// SkEdgeClipper.cpp's cubic half: a y- and x-monotone cubic is cut against the
// clip rectangle, the parts outside it becoming vertical edges.
#include "web_canvas/raster_clip.h"

#include "web_canvas/raster_edge.h"

namespace ruyijs::web_canvas::raster {
void EdgeClipper::ClipMonoCubic(const Point src[4], const ClipRect& clip) {
  Point pts[4];
  bool reverse = SortIncreasingY(pts, src, 4);
  if (pts[3].y <= clip.top || pts[0].y >= clip.bottom) return;
  ChopCubicInY(pts, clip);
  if (pts[0].x > pts[3].x) {
    for (int i = 0; i < 2; ++i) {
      const Point swap = pts[i];
      pts[i] = pts[3 - i];
      pts[3 - i] = swap;
    }
    reverse = !reverse;
  }
  if (pts[3].x <= clip.left) {  // wholly to the left
    AppendVLine(clip.left, pts[0].y, pts[3].y, reverse);
    return;
  }
  if (pts[0].x >= clip.right) {  // wholly to the right
    if (!CanCullToTheRight()) AppendVLine(clip.right, pts[0].y, pts[3].y, reverse);
    return;
  }
  Point tmp[7];
  if (pts[0].x < clip.left) {
    ChopMonoCubicAtX(pts, clip.left, tmp);
    AppendVLine(clip.left, tmp[0].y, tmp[3].y, reverse);
    tmp[3].x = clip.left;
    ClampGe(&tmp[4].x, clip.left);
    pts[0] = tmp[3];
    pts[1] = tmp[4];
    pts[2] = tmp[5];
  }
  if (pts[3].x > clip.right) {
    ChopMonoCubicAtX(pts, clip.right, tmp);
    tmp[3].x = clip.right;
    ClampLe(&tmp[2].x, clip.right);
    AppendCubic(tmp, reverse);
    AppendVLine(clip.right, tmp[3].y, tmp[6].y, reverse);
  } else {
    AppendCubic(pts, reverse);
  }
}

bool EdgeClipper::ClipCubic(const Point srcPts[4], const ClipRect& clip) {
  Reset();
  float top = srcPts[0].y, bottom = top;
  for (int i = 1; i < 4; ++i) {
    if (srcPts[i].y < top) top = srcPts[i].y;
    if (srcPts[i].y > bottom) bottom = srcPts[i].y;
  }
  if (bottom > clip.top && top < clip.bottom) {
    Point monoY[10];
    const int countY = ChopCubicAtYExtrema(srcPts, monoY);
    for (int y = 0; y <= countY; ++y) {
      Point monoX[10];
      const int countX = ChopCubicAtXExtrema(&monoY[y * 3], monoX);
      for (int x = 0; x <= countX; ++x) ClipMonoCubic(&monoX[x * 3], clip);
    }
  }
  verbStop_ = verbCount_;
  return verbStop_ != 0;
}
}  // namespace ruyijs::web_canvas::raster
