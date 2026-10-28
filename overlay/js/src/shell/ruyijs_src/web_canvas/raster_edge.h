#ifndef RUYIJS_WEB_CANVAS_RASTER_EDGE_H
#define RUYIJS_WEB_CANVAS_RASTER_EDGE_H
// One edge of Skia's analytic scan converter (src/core/SkAnalyticEdge.h): a
// y-monotone piece of a line, quadratic or cubic, forward differenced in 16.16
// fixed point. The fields of all three kinds share one struct so the walkers can
// treat an edge list uniformly, exactly like the Skia classes do through their
// common base.
#include "web_canvas/raster_fixed.h"
#include "web_canvas/raster_path.h"

namespace ruyijs::web_canvas::raster {
enum EdgeType : uint8_t { kLineEdge = 0, kQuadEdge, kCubicEdge };

struct Edge {
  Edge* next = nullptr;
  Edge* prev = nullptr;
  Fixed x = 0;
  Fixed dx = 0;
  Fixed upperX = 0;
  Fixed y = 0;
  Fixed upperY = 0;
  Fixed lowerY = 0;
  Fixed dy = 0;
  EdgeType type = kLineEdge;
  int8_t curveCount = 0;
  uint8_t curveShift = 0;
  int8_t winding = 1;
  // Quadratic forward differencing.
  Fixed qx = 0, qy = 0, qdx = 0, qdy = 0, qddx = 0, qddy = 0;
  Fixed qlastX = 0, qlastY = 0, snappedX = 0, snappedY = 0;
  // Cubic forward differencing.
  Fixed cx = 0, cy = 0, cdx = 0, cdy = 0, cddx = 0, cddy = 0, cdddx = 0, cdddy = 0;
  Fixed clastX = 0, clastY = 0, cSnappedY = 0;
  uint8_t cubicDShift = 0;

  // SkAnalyticEdge::goY: move the edge to y, keeping fX in step.
  void GoY(Fixed toY) {
    if (toY == y + kFixed1) {
      x = x + dx;
      y = toY;
    } else if (toY != y) {
      x = upperX + FixedMul(dx, toY - upperY);
      y = toY;
    }
  }
  void GoY(Fixed toY, int yShift) {
    y = toY;
    x += dx >> yShift;
  }
  bool Update(Fixed lastY);
  void KeepContinuous();
};

bool SetLine(Edge& edge, Point p0, Point p1);
// The curve setup returns false when the curve is flat or its first
// subdivision is degenerate, which drops the edge, exactly as Skia does.
bool SetQuadratic(Edge& edge, const Point points[3]);
bool SetCubic(Edge& edge, const Point points[4]);
// One forward-difference step of each curve kind (SkAnalytic*Edge::update*).
bool UpdateQuadratic(Edge& edge);
bool UpdateCubic(Edge& edge);
bool UpdateLine(Edge& edge, Fixed x0, Fixed y0, Fixed x1, Fixed y1, Fixed slope);

// raster_chop.cpp: Skia splits curves at their y extrema before edge building.
int ChopQuadAtYExtrema(const Point src[3], Point dst[5]);
int ChopCubicAtYExtrema(const Point src[4], Point dst[10]);
}  // namespace ruyijs::web_canvas::raster
#endif
