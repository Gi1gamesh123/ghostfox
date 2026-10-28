// SkEdgeClipper.cpp's quad half: a y- and x-monotone quad is cut against the
// clip rectangle, the parts outside it becoming vertical edges.
#include "web_canvas/raster_clip.h"

#include "web_canvas/raster_edge.h"

namespace ruyijs::web_canvas::raster {
void EdgeClipper::ClipMonoQuad(const Point srcPts[3], const ClipRect& clip) {
  Point pts[3];
  bool reverse = SortIncreasingY(pts, srcPts, 3);
  if (pts[2].y <= clip.top || pts[0].y >= clip.bottom) return;
  ChopQuadInY(pts, clip);
  if (pts[0].x > pts[2].x) {
    const Point swap = pts[0];
    pts[0] = pts[2];
    pts[2] = swap;
    reverse = !reverse;
  }
  if (pts[2].x <= clip.left) {  // wholly to the left
    AppendVLine(clip.left, pts[0].y, pts[2].y, reverse);
    return;
  }
  if (pts[0].x >= clip.right) {  // wholly to the right
    if (!CanCullToTheRight()) AppendVLine(clip.right, pts[0].y, pts[2].y, reverse);
    return;
  }
  Point tmp[5];
  if (pts[0].x < clip.left) {
    ChopQuadAt(pts, tmp, BisectQuadAt(pts[0].x, pts[1].x, pts[2].x, clip.left));
    AppendVLine(clip.left, tmp[0].y, tmp[2].y, reverse);
    tmp[2].x = clip.left;
    ClampGe(&tmp[3].x, clip.left);
    pts[0] = tmp[2];
    pts[1] = tmp[3];
  }
  if (pts[2].x > clip.right) {
    ChopQuadAt(pts, tmp, BisectQuadAt(pts[0].x, pts[1].x, pts[2].x, clip.right));
    ClampLe(&tmp[1].x, clip.right);
    tmp[2].x = clip.right;
    AppendQuad(tmp, reverse);
    AppendVLine(clip.right, tmp[2].y, tmp[4].y, reverse);
  } else {
    AppendQuad(pts, reverse);
  }
}

bool EdgeClipper::ClipQuad(const Point srcPts[3], const ClipRect& clip) {
  Reset();
  Point monoY[5];
  const int countY = ChopQuadAtYExtrema(srcPts, monoY);
  for (int y = 0; y <= countY; ++y) {
    Point monoX[5];
    const int countX = ChopQuadAtXExtrema(&monoY[y * 2], monoX);
    for (int x = 0; x <= countX; ++x) ClipMonoQuad(&monoX[x * 2], clip);
  }
  verbStop_ = verbCount_;
  return verbStop_ != 0;
}
}  // namespace ruyijs::web_canvas::raster
