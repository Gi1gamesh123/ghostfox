// SkEdgeBuilder + SkPathEdgeIter: the path becomes y-monotone analytic edges
// (curves are split at their y extrema first). The edge list order and the
// vertical-edge merge decide how the walkers pair edges up, so both are kept.
#include <memory>
#include <vector>

#include "web_canvas/raster_build.h"

namespace ruyijs::web_canvas::raster {
namespace {
int Abs32(int32_t v) { return v < 0 ? -v : v; }
bool ApproximatelyEqual(Fixed a, Fixed b) { return Abs32(a - b) < 0x100; }

enum Combine { kNoCombine = 0, kPartialCombine, kTotalCombine };

// SkAnalyticEdgeBuilder::combineVertical: two vertical edges of one contour are
// one edge, or two that cancel out.
Combine CombineVertical(Edge* edge, Edge* last) {
  if (last->type != kLineEdge || last->dx || edge->x != last->x) return kNoCombine;
  if (edge->winding == last->winding) {
    if (edge->lowerY == last->upperY) {
      last->upperY = edge->upperY;
      last->y = last->upperY;
      return kPartialCombine;
    }
    if (ApproximatelyEqual(edge->upperY, last->lowerY)) {
      last->lowerY = edge->lowerY;
      return kPartialCombine;
    }
    return kNoCombine;
  }
  if (ApproximatelyEqual(edge->upperY, last->upperY)) {
    if (ApproximatelyEqual(edge->lowerY, last->lowerY)) return kTotalCombine;
    if (edge->lowerY < last->lowerY) {
      last->upperY = edge->lowerY;
      last->y = last->upperY;
      return kPartialCombine;
    }
    last->upperY = last->lowerY;
    last->y = last->upperY;
    last->lowerY = edge->lowerY;
    last->winding = edge->winding;
    return kPartialCombine;
  }
  if (ApproximatelyEqual(edge->lowerY, last->lowerY)) {
    if (edge->upperY > last->upperY) {
      last->lowerY = edge->upperY;
      return kPartialCombine;
    }
    last->lowerY = last->upperY;
    last->upperY = edge->upperY;
    last->y = last->upperY;
    last->winding = edge->winding;
    return kPartialCombine;
  }
  return kNoCombine;
}

// SkPathEdgeIter: segments only, with every contour auto-closed. The returned
// pointer starts at the segment's own start point.
struct SegmentIter {
  const Path& path;
  size_t verb = 0;
  size_t point = 0;
  Point moveTo;
  Point current;
  bool needsClose = false;
  Point scratch[2];

  const Point* CloseLine() {
    scratch[0] = current;
    scratch[1] = moveTo;
    needsClose = false;
    return scratch;
  }
  const Point* Next(uint8_t* kind) {
    for (;;) {
      if (verb == path.verbs.size()) {
        if (!needsClose) return nullptr;
        *kind = kLine;
        return CloseLine();
      }
      const uint8_t next = path.verbs[verb++];
      if (next == kMove) {
        moveTo = path.points[point++];
        if (needsClose) {
          *kind = kLine;
          const Point* closed = CloseLine();
          current = moveTo;
          return closed;
        }
        current = moveTo;
      } else if (next == kClose) {
        if (needsClose) {
          *kind = kLine;
          return CloseLine();
        }
      } else {
        const size_t count = next == kLine ? 1 : next == kQuad ? 2 : 3;
        const Point* from = &path.points[point - 1];
        point += count;
        needsClose = true;
        current = path.points[point - 1];
        *kind = next;
        return from;
      }
    }
  }
};
}  // namespace

// SkAnalyticEdgeBuilder::addLine: a vertical edge merges with the one before it.
void AddLine(EdgePool* pool, Point p0, Point p1) {
  Edge* edge = pool->New();
  if (!SetLine(*edge, p0, p1)) return;
  const bool vertical = edge->dx == 0 && edge->type == kLineEdge;
  const Combine combine =
      vertical && !pool->edges.empty() ? CombineVertical(edge, pool->edges.back()) : kNoCombine;
  if (combine == kTotalCombine) pool->edges.pop_back();
  else if (combine == kNoCombine) pool->Push(edge);
}

void BuildEdges(const Path& path, EdgePool* pool, const ClipRect* clip) {
  SegmentIter iter{path};
  // SkAnalyticEdgeBuilder::buildEdges: with a clip the segments go through
  // SkEdgeClipper, whose (line/quad/cubic) verbs then become edges.
  EdgeClipper clipper(!IsConvexPath(path));
  Point clipped[4];
  Point chopped[10];
  uint8_t kind = kLine;
  while (const Point* points = iter.Next(&kind)) {
    if (clip) {
      bool any = false;
      if (kind == kLine) any = clipper.ClipLine(points[0], points[1], *clip);
      else if (kind == kQuad) any = clipper.ClipQuad(points, *clip);
      else any = clipper.ClipCubic(points, *clip);
      if (any) {
        int verb;
        while ((verb = clipper.Next(clipped)) >= 0) {
          if (verb == kLine) {
            AddLine(pool, clipped[0], clipped[1]);
          } else if (verb == kQuad) {
            const int roots = ChopQuadAtYExtrema(clipped, chopped);
            for (int i = 0; i <= roots; ++i) {
              Edge* edge = pool->New();
              if (SetQuadratic(*edge, &chopped[i * 2])) pool->Push(edge);
            }
          } else {
            const int roots = ChopCubicAtYExtrema(clipped, chopped);
            for (int i = 0; i <= roots; ++i) {
              Edge* edge = pool->New();
              if (SetCubic(*edge, &chopped[i * 3])) pool->Push(edge);
            }
          }
        }
      }
      continue;
    }
    if (kind == kLine) {
      AddLine(pool, points[0], points[1]);
      continue;
    }
    if (kind == kQuad) {
      const int roots = ChopQuadAtYExtrema(points, chopped);
      for (int i = 0; i <= roots; ++i) {
        Edge* edge = pool->New();
        if (SetQuadratic(*edge, &chopped[i * 2])) pool->Push(edge);
      }
      continue;
    }
    const int roots = ChopCubicAtYExtrema(points, chopped);
    for (int i = 0; i <= roots; ++i) {
      Edge* edge = pool->New();
      if (SetCubic(*edge, &chopped[i * 3])) pool->Push(edge);
    }
  }
}
}  // namespace ruyijs::web_canvas::raster
