// SkScan::AntiFillPath + SkScan::AAAFillPath: bounds, the additive blitter the
// path size selects, the convex/general walker dispatch and the run flush that
// ends up in the canvas byte store.
#include <algorithm>
#include <cmath>
#include <vector>

#include "web_canvas/raster_blitter.h"
#include "web_canvas/raster_build.h"
#include "web_canvas/raster_path.h"
#include "web_canvas/raster_trapezoid.h"
#include "web_canvas/raster_walk.h"

namespace ruyijs::web_canvas::raster {
namespace {
bool CompareEdges(const Edge* a, const Edge* b) {
  if (a->upperY != b->upperY) return a->upperY < b->upperY;
  if (a->x != b->x) return a->x < b->x;
  return a->dx < b->dx;
}
// rect_overflows_short_shift(rect, SK_SUPERSAMPLE_SHIFT): Skia drops anti-aliasing
// for coordinates that do not fit a short after the supersample shift.
bool OverflowsShortShift(int left, int top, int right, int bottom) {
  const int64_t limit = 8191;
  return left < -limit || top < -limit || right > limit || bottom > limit;
}
// MaskAdditiveBlitter::CanHandleRect: the mask holds a path at most 32 wide and
// at most 1 KiB of 4-byte-aligned rows.
bool MaskFitsPath(int left, int top, int right, int bottom) {
  const int width = right - left;
  if (width <= 0 || width > 32) return false;
  const int64_t storage = int64_t((width + 3) & ~3) * int64_t(bottom - top);
  return storage <= 1024;
}
// aaa_fill_path picks the walker: the convex one for a convex path, the general
// one with the head and tail sentinels otherwise.
void Walk(Edge* head, Edge* tail, const Path& path, bool convex, AdditiveBlitter& additive,
          int startY, int stopY, Fixed leftBound, Fixed rightBound, size_t height) {
  if (convex) {
    WalkConvexEdges(head->next, additive, stopY, leftBound, rightBound);
    return;
  }
  const bool skipIntersect = path.points.size() > height * 2;
  WalkEdges(head, tail, path.evenOdd, additive, startY, stopY, leftBound, rightBound, false,
            skipIntersect);
}
}  // namespace

void FillPath(const Path& path, Surface surface, const Paint& paint) {
  if (!surface.pixels || surface.width <= 0 || surface.height <= 0) return;
  if (path.points.empty()) return;
  float minX = path.points[0].x, maxX = minX;
  float minY = path.points[0].y, maxY = minY;
  for (const Point& point : path.points) {
    minX = std::min(minX, point.x);
    maxX = std::max(maxX, point.x);
    minY = std::min(minY, point.y);
    maxY = std::max(maxY, point.y);
  }
  if (!std::isfinite(minX) || !std::isfinite(maxX) || !std::isfinite(minY) ||
      !std::isfinite(maxY)) {
    return;
  }
  // SkScan_AntiPath's safeRoundOut: the bounds the clip test sees are rounded out.
  const int left = int(std::floor(minX)), top = int(std::floor(minY));
  const int right = int(std::ceil(maxX)), bottom = int(std::ceil(maxY));
  if (right <= left || bottom <= top) return;
  if (OverflowsShortShift(left, top, right, bottom)) return;
  const bool contained = left >= 0 && top >= 0 && right <= surface.width &&
                         bottom <= surface.height;
  // SkScanClipper wraps the blitter in SkRectClipBlitter when the path sticks out
  // past the clip horizontally; that wrapper has no blitAntiH2, so those corners
  // reach the run blitter instead of the opaque blitter's fast interpolator.
  const bool wrapped = !contained && (left < 0 || right > surface.width);
  const int startY = std::max(top, 0);
  const int stopY = std::min(bottom, surface.height);
  if (stopY <= startY) return;
  int sectionLeft = std::max(left, 0);
  int sectionTop = std::max(top, 0);
  const int sectionRight = std::min(right, surface.width);
  const int sectionBottom = std::min(bottom, surface.height);
  if (sectionRight <= sectionLeft || sectionBottom <= sectionTop) return;

  EdgePool pool;
  // aaa_fill_path hands the clipper the clip bounds, not the path bounds.
  const ClipRect clip{0.f, 0.f, float(surface.width), float(surface.height)};
  BuildEdges(path, &pool, contained ? nullptr : &clip);
  if (pool.edges.empty()) return;
  std::sort(pool.edges.begin(), pool.edges.end(), CompareEdges);
  for (size_t i = 0; i < pool.edges.size(); ++i) {
    pool.edges[i]->prev = i == 0 ? nullptr : pool.edges[i - 1];
    pool.edges[i]->next = i + 1 == pool.edges.size() ? nullptr : pool.edges[i + 1];
  }
  Edge head, tail;
  head.prev = nullptr;
  head.next = pool.edges.front();
  head.upperY = head.lowerY = kMinS32;
  head.x = kMinS32;
  head.dx = 0;
  head.dy = kMaxS32;
  head.upperX = kMinS32;
  pool.edges.front()->prev = &head;
  tail.prev = pool.edges.back();
  tail.next = nullptr;
  tail.upperY = tail.lowerY = kMaxS32;
  tail.x = kMaxS32;
  tail.dx = 0;
  tail.dy = kMaxS32;
  tail.upperX = kMaxS32;
  pool.edges.back()->next = &tail;

  Blitter blitter(surface, paint, wrapped);
  const Fixed leftBound = IntToFixed(sectionLeft);
  const Fixed rightBound = IntToFixed(sectionRight);
  const size_t height = size_t(sectionBottom - sectionTop);
  const bool convex = IsConvexPath(path) && pool.edges.size() >= 2;
  // SkScan::AAAFillPath: a path whose bounds fit MaskAdditiveBlitter accumulates
  // its alphas in an A8 mask that is blitted once, and its alphas are never
  // snapped to 0 or 255 the way the run based blitter snaps them.
  std::vector<uint8_t> mask;
  if (MaskFitsPath(left, top, right, bottom)) {
    mask.assign(size_t(right - left) * size_t(bottom - top), 0);
    AdditiveBlitter additive(blitter, mask.data(), left, top, right - left, bottom - top);
    Walk(&head, &tail, path, convex, additive, startY, stopY, leftBound, rightBound, height);
    BlitMask(surface, paint, left, top, mask.data(), right - left, bottom - top);
    return;
  }
  AdditiveBlitter additive(blitter, sectionLeft, sectionRight - sectionLeft, sectionTop,
                           !convex);
  Walk(&head, &tail, path, convex, additive, startY, stopY, leftBound, rightBound, height);
}
}  // namespace ruyijs::web_canvas::raster
