// SkPathPriv::ComputeConvexity: Skia decides between its convex and its general
// anti-aliased walker from this answer, and the two walkers quantise differently,
// so the test is reproduced rather than approximated.
#include <cmath>

#include "web_canvas/raster_build.h"

namespace ruyijs::web_canvas::raster {
namespace {
struct Vector {
  float x = 0.0f;
  float y = 0.0f;
};
constexpr int kSignNeverReturned = 2;
enum DirChange { kUnknown, kLeft, kRight, kStraight, kBackwards, kInvalid };

int Sign(float x) { return x < 0 ? 1 : 0; }
bool Zero(Vector v) { return v.x == 0 && v.y == 0; }
Vector Sub(Point a, Point b) { return Vector{a.x - b.x, a.y - b.y}; }

struct Convexicator {
  Point firstPt, lastPt;
  Vector lastVec, firstVec;
  DirChange expectedDir = kInvalid;
  int reversals = 0;
  int firstDirection = 0;  // 0 unknown, 1 clockwise, 2 counter-clockwise
  bool isFinite = true;

  void SetMovePt(Point pt) {
    firstPt = lastPt = pt;
    expectedDir = kInvalid;
  }
  DirChange DirectionChange(Vector current) const {
    const float cross = lastVec.x * current.y - lastVec.y * current.x;
    if (!std::isfinite(cross)) return kUnknown;
    if (cross == 0) {
      return lastVec.x * current.x + lastVec.y * current.y < 0 ? kBackwards : kStraight;
    }
    return cross > 0 ? kRight : kLeft;
  }
  bool AddVector(Vector current) {
    const DirChange dir = DirectionChange(current);
    switch (dir) {
      case kLeft:
      case kRight:
        if (expectedDir == kInvalid) {
          expectedDir = dir;
          firstDirection = dir == kRight ? 1 : 2;
        } else if (dir != expectedDir) {
          firstDirection = 0;
          return false;
        }
        lastVec = current;
        return true;
      case kStraight:
        return true;
      case kBackwards:
        lastVec = current;
        return ++reversals < 3;
      case kUnknown:
        isFinite = false;
        return false;
      case kInvalid:
        return false;
    }
    return false;
  }
  bool AddPoint(Point pt) {
    if (lastPt.x == pt.x && lastPt.y == pt.y) return true;
    if (firstPt.x == lastPt.x && firstPt.y == lastPt.y && expectedDir == kInvalid &&
        Zero(lastVec)) {
      lastVec = Sub(pt, lastPt);
      firstVec = lastVec;
    } else if (!AddVector(Sub(pt, lastPt))) {
      return false;
    }
    lastPt = pt;
    return true;
  }
  bool Close() { return AddPoint(firstPt) && AddVector(firstVec); }
};

// IsConcaveBySign: a quick test for a path that turns more than three times.
bool IsConcaveBySign(const std::vector<Point>& points) {
  if (points.size() <= 3) return false;
  const Point* last = points.data() + points.size();
  Point currPt = *points.data();
  const Point* cursor = points.data() + 1;
  const Point firstPt = currPt;
  int dxes = 0, dyes = 0;
  int lastSx = kSignNeverReturned, lastSy = kSignNeverReturned;
  for (int outer = 0; outer < 2; ++outer) {
    while (cursor != last) {
      const Vector vec = Sub(*cursor, currPt);
      if (!Zero(vec)) {
        if (!std::isfinite(vec.x) || !std::isfinite(vec.y)) return true;
        const int sx = Sign(vec.x), sy = Sign(vec.y);
        dxes += (sx != lastSx);
        dyes += (sy != lastSy);
        if (dxes > 3 || dyes > 3) return true;
        lastSx = sx;
        lastSy = sy;
      }
      currPt = *cursor++;
      if (outer) break;
    }
    cursor = &firstPt;
  }
  return false;
}
}  // namespace

bool IsConvexPath(const Path& path) {
  // trim_trailing_moves: trailing move verbs carry no geometry.
  size_t verbCount = path.verbs.size();
  size_t pointCount = path.points.size();
  while (verbCount > 0 && path.verbs[verbCount - 1] == kMove) {
    --verbCount;
    --pointCount;
  }
  if (verbCount == 0) return true;
  if (IsConcaveBySign(path.points)) return false;
  int contourCount = 0;
  bool needsClose = false;
  Convexicator state;
  size_t pointIndex = 0;
  for (size_t i = 0; i < verbCount; ++i) {
    const uint8_t verb = path.verbs[i];
    if (contourCount == 0) {
      if (verb == kMove) {
        state.SetMovePt(path.points[pointIndex]);
      } else {
        ++contourCount;
        needsClose = true;
      }
    }
    if (contourCount == 1) {
      if (verb == kClose || verb == kMove) {
        if (!state.Close()) return false;
        needsClose = false;
        ++contourCount;
      } else {
        const int count = verb == kLine ? 1 : verb == kQuad ? 2 : 3;
        for (int k = 0; k < count; ++k) {
          if (!state.AddPoint(path.points[pointIndex + size_t(k)])) return false;
        }
      }
    } else if (verb != kMove) {
      // A second contour can never be convex.
      return false;
    }
    pointIndex += verb == kMove || verb == kLine ? 1 : verb == kQuad ? 2 : verb == kCubic ? 3 : 0;
  }
  (void)pointCount;
  if (needsClose && !state.Close()) return false;
  if (state.firstDirection == 0 && state.reversals >= 3) return false;
  return true;
}
}  // namespace ruyijs::web_canvas::raster
