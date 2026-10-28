// SkAnalyticCubicEdge: the cubic form of Skia's analytic edges, plus the
// keepContinuous() fixup the concave walker applies between rows.
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

int DiffToShift(FDot6 dx, FDot6 dy, int shiftAA) {
  FDot6 dist = CheapDistance(dx, dy);
  dist = (dist + (1 << (2 + shiftAA))) >> (3 + shiftAA);
  int bits = 0;
  while (dist != 0) { ++bits; dist >>= 1; }
  return bits >> 1;
}

int FDot6UpShift(FDot6 x, int upShift) { return x << upShift; }

// cubic_delta_from_line: how far the curve wanders from its chord.
FDot6 CubicDeltaFromLine(FDot6 a, FDot6 b, FDot6 c, FDot6 d) {
  const FDot6 oneThird = (a * 8 - b * 15 + 6 * c + d) * 19 >> 9;
  const FDot6 twoThird = (a + 6 * b - c * 15 + d * 8) * 19 >> 9;
  return Abs32(oneThird) > Abs32(twoThird) ? Abs32(oneThird) : Abs32(twoThird);
}
}  // namespace

bool SetCubic(Edge& edge, const Point points[4]) {
  constexpr int shiftAA = kDefaultAccuracy;
  FDot6 x0 = ScalarRoundToFDot6(points[0].x, shiftAA);
  FDot6 y0 = ScalarRoundToFDot6(points[0].y, shiftAA);
  FDot6 x1 = ScalarRoundToFDot6(points[1].x, shiftAA);
  FDot6 y1 = ScalarRoundToFDot6(points[1].y, shiftAA);
  FDot6 x2 = ScalarRoundToFDot6(points[2].x, shiftAA);
  FDot6 y2 = ScalarRoundToFDot6(points[2].y, shiftAA);
  FDot6 x3 = ScalarRoundToFDot6(points[3].x, shiftAA);
  FDot6 y3 = ScalarRoundToFDot6(points[3].y, shiftAA);
  int8_t winding = 1;
  if (y0 > y3) {
    FDot6 swap = x0; x0 = x3; x3 = swap;
    swap = x1; x1 = x2; x2 = swap;
    swap = y0; y0 = y3; y3 = swap;
    swap = y1; y1 = y2; y2 = swap;
    winding = -1;
  }
  if (FDot6Round(y0) == FDot6Round(y3)) return false;
  int steps = DiffToShift(CubicDeltaFromLine(x0, x1, x2, x3),
                          CubicDeltaFromLine(y0, y1, y2, y3), 2) + 1;
  if (steps > kMaxCoeffShift) steps = kMaxCoeffShift;
  int upShift = 6;
  int downShift = steps + upShift - 10;
  if (downShift < 0) {
    downShift = 0;
    upShift = 10 - steps;
  }
  edge.winding = winding;
  edge.type = kCubicEdge;
  edge.curveCount = int8_t(-1 << steps);
  edge.curveShift = uint8_t(steps);
  edge.cubicDShift = uint8_t(downShift);
  Fixed b = FDot6UpShift(3 * (x1 - x0), upShift);
  Fixed c = FDot6UpShift(3 * (x0 - x1 - x1 + x2), upShift);
  Fixed d = FDot6UpShift(x3 + 3 * (x1 - x2) - x0, upShift);
  edge.cx = FDot6ToFixed(x0);
  edge.cdx = b + (c >> steps) + (d >> 2 * steps);
  edge.cddx = 2 * c + (3 * d >> (steps - 1));
  edge.cdddx = 3 * d >> (steps - 1);
  b = FDot6UpShift(3 * (y1 - y0), upShift);
  c = FDot6UpShift(3 * (y0 - y1 - y1 + y2), upShift);
  d = FDot6UpShift(y3 + 3 * (y1 - y2) - y0, upShift);
  edge.cy = FDot6ToFixed(y0);
  edge.cdy = b + (c >> steps) + (d >> 2 * steps);
  edge.cddy = 2 * c + (3 * d >> (steps - 1));
  edge.cdddy = 3 * d >> (steps - 1);
  edge.clastX = FDot6ToFixed(x3);
  edge.clastY = FDot6ToFixed(y3);
  edge.cx >>= shiftAA; edge.cy >>= shiftAA; edge.cdx >>= shiftAA; edge.cdy >>= shiftAA;
  edge.cddx >>= shiftAA; edge.cddy >>= shiftAA;
  edge.cdddx >>= shiftAA; edge.cdddy >>= shiftAA;
  edge.clastX >>= shiftAA; edge.clastY >>= shiftAA;
  edge.cy = SnapY(edge.cy);
  edge.cSnappedY = edge.cy;
  edge.clastY = SnapY(edge.clastY);
  return UpdateCubic(edge);
}

bool UpdateCubic(Edge& edge) {
  int count = edge.curveCount;
  Fixed oldx = edge.cx, oldy = edge.cy;
  Fixed newx = 0, newy = 0;
  const int ddshift = edge.curveShift;
  const int dshift = edge.cubicDShift;
  int success;
  do {
    if (++count < 0) {
      newx = oldx + (edge.cdx >> dshift);
      edge.cdx += edge.cddx >> ddshift;
      edge.cddx += edge.cdddx;
      newy = oldy + (edge.cdy >> dshift);
      edge.cdy += edge.cddy >> ddshift;
      edge.cddy += edge.cdddy;
    } else {
      newx = edge.clastX;
      newy = edge.clastY;
    }
    if (newy < oldy) newy = oldy;
    Fixed newSnappedY = SnapY(newy);
    if (edge.clastY < newSnappedY) {
      newSnappedY = edge.clastY;
      count = 0;
    }
    const Fixed slope = FixedToFDot6(newSnappedY - edge.cSnappedY) == 0
                            ? kMaxS32
                            : FDot6Div(FixedToFDot6(newx - oldx),
                                       FixedToFDot6(newSnappedY - edge.cSnappedY));
    success = UpdateLine(edge, oldx, edge.cSnappedY, newx, newSnappedY, slope);
    oldx = newx;
    oldy = newy;
    edge.cSnappedY = newSnappedY;
  } while (count < 0 && !success);
  edge.cx = newx;
  edge.cy = newy;
  edge.curveCount = int8_t(count);
  return success != 0;
}

// keepContinuous(): the next subdivision must start where the last one ended,
// otherwise the sorted edge list can break.
void Edge::KeepContinuous() {
  if (curveCount < 0) {
    cx = x;
    cSnappedY = y;
  } else if (curveCount > 0) {
    snappedX = x;
    snappedY = y;
  }
}
}  // namespace ruyijs::web_canvas::raster
