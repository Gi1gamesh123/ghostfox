// SkGeometry.cpp's x-extrema splits: the same chop as the y-extrema ones with
// the two coordinates exchanged, which is exactly what Skia's separate
// ChopQuadAtXExtrema/ChopCubicAtXExtrema do.
#include "web_canvas/raster_clip.h"
#include "web_canvas/raster_edge.h"

namespace ruyijs::web_canvas::raster {
namespace {
void SwapAxes(Point* points, int count) {
  for (int i = 0; i < count; ++i) points[i] = Point{points[i].y, points[i].x};
}
}  // namespace

int ChopQuadAtXExtrema(const Point src[3], Point dst[5]) {
  Point swapped[3] = {Point{src[0].y, src[0].x}, Point{src[1].y, src[1].x},
                      Point{src[2].y, src[2].x}};
  const int roots = ChopQuadAtYExtrema(swapped, dst);
  SwapAxes(dst, roots == 0 ? 3 : 5);
  return roots;
}

int ChopCubicAtXExtrema(const Point src[4], Point dst[10]) {
  Point swapped[4] = {Point{src[0].y, src[0].x}, Point{src[1].y, src[1].x},
                      Point{src[2].y, src[2].x}, Point{src[3].y, src[3].x}};
  const int roots = ChopCubicAtYExtrema(swapped, dst);
  SwapAxes(dst, roots == 0 ? 4 : 10);
  return roots;
}
}  // namespace ruyijs::web_canvas::raster
