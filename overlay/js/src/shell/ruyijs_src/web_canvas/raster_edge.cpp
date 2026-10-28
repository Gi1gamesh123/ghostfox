// SkAnalyticEdge.cpp: the line and quadratic edge setup and forward
// differencing that Skia's analytic anti-aliasing walks. Everything is 16.16
// fixed point; the y values are snapped to a quarter pixel.
#include "web_canvas/raster_edge.h"

namespace ruyijs::web_canvas::raster {
namespace {
constexpr int kDefaultAccuracy = 2;
constexpr int kMaxCoeffShift = 6;

int Abs32(int32_t v) { return v < 0 ? -v : v; }

int CheapDistance(FDot6 dx, FDot6 dy) {
  dx = dx < 0 ? -dx : dx;
  dy = dy < 0 ? -dy : dy;
  return dx > dy ? dx + (dy >> 1) : dy + (dx >> 1);
}

// diff_to_shift: how many subdivisions the curve needs (1 << shift pieces).
int DiffToShift(FDot6 dx, FDot6 dy, int shiftAA) {
  FDot6 dist = CheapDistance(dx, dy);
  dist = (dist + (1 << (2 + shiftAA))) >> (3 + shiftAA);
  int bits = 0;
  while (dist != 0) { ++bits; dist >>= 1; }
  return bits >> 1;
}
}  // namespace

bool Edge::Update(Fixed lastY) {
  if (curveCount < 0) return UpdateCubic(*this);
  if (curveCount > 0) return UpdateQuadratic(*this);
  return false;
}

bool SetLine(Edge& edge, Point p0, Point p1) {
  Fixed x0 = FDot6ToFixed(ScalarRoundToFDot6(p0.x, kDefaultAccuracy)) >> kDefaultAccuracy;
  Fixed y0 = SnapY(FDot6ToFixed(ScalarRoundToFDot6(p0.y, kDefaultAccuracy)) >> kDefaultAccuracy);
  Fixed x1 = FDot6ToFixed(ScalarRoundToFDot6(p1.x, kDefaultAccuracy)) >> kDefaultAccuracy;
  Fixed y1 = SnapY(FDot6ToFixed(ScalarRoundToFDot6(p1.y, kDefaultAccuracy)) >> kDefaultAccuracy);
  int8_t winding = 1;
  if (y0 > y1) {
    Fixed swap = x0; x0 = x1; x1 = swap;
    swap = y0; y0 = y1; y1 = swap;
    winding = -1;
  }
  if (FixedToFDot6(y1 - y0) == 0) return false;
  const FDot6 dx = FixedToFDot6(x1 - x0);
  const FDot6 dy = FixedToFDot6(y1 - y0);
  const Fixed slope = QuickDiv(dx, dy);
  // SkAnalyticEdge::setLine keeps absSlope in Fixed (unlike updateLine, which
  // uses FDot6), so a straight edge only takes the inverse-table shortcut when
  // its Fixed slope is below 1024 and otherwise divides dy by dx.
  const Fixed absSlope = Abs32(slope);
  edge.x = x0;
  edge.dx = slope;
  edge.upperX = x0;
  edge.y = y0;
  edge.upperY = y0;
  edge.lowerY = y1;
  edge.dy = dx == 0 || slope == 0 ? kMaxS32
             : absSlope < 1024 ? QuickInverse(absSlope) : Abs32(QuickDiv(dy, dx));
  edge.type = kLineEdge;
  edge.curveCount = 0;
  edge.winding = winding;
  edge.curveShift = 0;
  return true;
}

// SkAnalyticEdge::updateLine: one curve subdivision becomes a straight edge.
bool UpdateLine(Edge& edge, Fixed x0, Fixed y0, Fixed x1, Fixed y1, Fixed slope) {
  if (y0 > y1) {
    Fixed swap = x0; x0 = x1; x1 = swap;
    swap = y0; y0 = y1; y1 = swap;
    edge.winding = int8_t(-edge.winding);
  }
  const FDot6 dx = FixedToFDot6(x1 - x0);
  const FDot6 dy = FixedToFDot6(y1 - y0);
  if (dy == 0) return false;
  const FDot6 absSlope = Abs32(FixedToFDot6(slope));
  edge.x = x0;
  edge.dx = slope;
  edge.upperX = x0;
  edge.y = y0;
  edge.upperY = y0;
  edge.lowerY = y1;
  edge.dy = dx == 0 || slope == 0 ? kMaxS32
             : absSlope < 1024 ? QuickInverse(absSlope) : Abs32(QuickDiv(dy, dx));
  return true;
}

bool SetQuadratic(Edge& edge, const Point points[3]) {
  constexpr int shift = kDefaultAccuracy;
  FDot6 x0 = ScalarRoundToFDot6(points[0].x, shift), y0 = ScalarRoundToFDot6(points[0].y, shift);
  FDot6 x1 = ScalarRoundToFDot6(points[1].x, shift), y1 = ScalarRoundToFDot6(points[1].y, shift);
  FDot6 x2 = ScalarRoundToFDot6(points[2].x, shift), y2 = ScalarRoundToFDot6(points[2].y, shift);
  int8_t winding = 1;
  if (y0 > y2) {
    FDot6 swap = x0; x0 = x2; x2 = swap;
    swap = y0; y0 = y2; y2 = swap;
    winding = -1;
  }
  if (FDot6Round(y0) == FDot6Round(y2)) return false;
  const FDot6 dx = ((x1 << 1) - x0 - x2) >> 2;
  const FDot6 dy = ((y1 << 1) - y0 - y2) >> 2;
  int steps = DiffToShift(dx, dy, shift);
  if (steps == 0) steps = 1;
  else if (steps > kMaxCoeffShift) steps = kMaxCoeffShift;
  edge.winding = winding;
  edge.type = kQuadEdge;
  edge.curveCount = int8_t(1 << steps);
  edge.curveShift = uint8_t(steps - 1);
  // A and B are stored at half scale, hence the (shift - 1) bias.
  Fixed a = FDot6ToFixedDiv2(x0 - x1 - x1 + x2);
  Fixed b = FDot6ToFixed(x1 - x0);
  edge.qx = FDot6ToFixed(x0);
  edge.qdx = b + (a >> steps);
  edge.qddx = a >> (steps - 1);
  a = FDot6ToFixedDiv2(y0 - y1 - y1 + y2);
  b = FDot6ToFixed(y1 - y0);
  edge.qy = FDot6ToFixed(y0);
  edge.qdy = b + (a >> steps);
  edge.qddy = a >> (steps - 1);
  edge.qlastX = FDot6ToFixed(x2);
  edge.qlastY = FDot6ToFixed(y2);
  edge.qx >>= shift; edge.qy >>= shift; edge.qdx >>= shift; edge.qdy >>= shift;
  edge.qddx >>= shift; edge.qddy >>= shift;
  edge.qlastX >>= shift; edge.qlastY >>= shift;
  edge.qy = SnapY(edge.qy);
  edge.qlastY = SnapY(edge.qlastY);
  edge.snappedX = edge.qx;
  edge.snappedY = edge.qy;
  return UpdateQuadratic(edge);
}

bool UpdateQuadratic(Edge& edge) {
  int count = edge.curveCount;
  Fixed oldx = edge.qx, oldy = edge.qy;
  Fixed dx = edge.qdx, dy = edge.qdy;
  Fixed newx = 0, newy = 0, newSnappedX = 0, newSnappedY = 0;
  const int shift = edge.curveShift;
  int success = 0;
  do {
    Fixed slope;
    if (--count > 0) {
      newx = oldx + (dx >> shift);
      newy = oldy + (dy >> shift);
      if (Abs32(dy >> shift) >= kFixed1 * 2 && (int64_t(Abs32(dy)) << 6) > Abs32(dx)) {
        const FDot6 diffY = FixedToFDot6(newy - edge.snappedY);
        slope = diffY ? QuickDiv(FixedToFDot6(newx - edge.snappedX), diffY) : kMaxS32;
        newSnappedY = edge.qlastY < RoundToFixed(newy) ? edge.qlastY : RoundToFixed(newy);
        newSnappedX = newx - FixedMul(slope, newy - newSnappedY);
      } else {
        newSnappedY = edge.qlastY < SnapY(newy) ? edge.qlastY : SnapY(newy);
        newSnappedX = newx;
        const FDot6 diffY = FixedToFDot6(newSnappedY - edge.snappedY);
        slope = diffY ? QuickDiv(FixedToFDot6(newx - edge.snappedX), diffY) : kMaxS32;
      }
      dx += edge.qddx;
      dy += edge.qddy;
    } else {
      newx = edge.qlastX;
      newy = edge.qlastY;
      newSnappedY = newy;
      newSnappedX = newx;
      const FDot6 diffY = FixedToFDot6(newy - edge.snappedY);
      slope = diffY ? QuickDiv(FixedToFDot6(newx - edge.snappedX), diffY) : kMaxS32;
    }
    if (slope < kMaxS32) {
      success = UpdateLine(edge, edge.snappedX, edge.snappedY, newSnappedX, newSnappedY, slope);
    }
    oldx = newx;
    oldy = newy;
  } while (count > 0 && !success);
  edge.qx = newx;
  edge.qy = newy;
  edge.qdx = dx;
  edge.qdy = dy;
  edge.snappedX = newSnappedX;
  edge.snappedY = newSnappedY;
  edge.curveCount = int8_t(count);
  return success != 0;
}
}  // namespace ruyijs::web_canvas::raster
