#ifndef RUYIJS_WEB_CANVAS_RASTER_CLIP_H
#define RUYIJS_WEB_CANVAS_RASTER_CLIP_H
// SkEdgeClipper + SkLineClipper: when a path is not contained in the clip, Skia
// clips every segment to the clip rectangle before building edges, replacing the
// parts outside it with vertical edges on the clip boundary. Skipping that (and
// letting the walker clip instead) changes the curve parameterisation, and with
// it the coverage of every row near the boundary.
#include "web_canvas/raster_path.h"

namespace ruyijs::web_canvas::raster {
struct ClipRect {
  float left = 0, top = 0, right = 0, bottom = 0;
};

// SkLineClipper: up to kMaxClippedPoints points, kMaxClippedLineSegments segments.
enum { kMaxClippedPoints = 4, kMaxClippedLineSegments = kMaxClippedPoints - 1 };
int ClipLineSegments(const Point pts[2], const ClipRect& clip, Point lines[kMaxClippedPoints],
                     bool canCullToTheRight);

class EdgeClipper {
 public:
  explicit EdgeClipper(bool canCullToTheRight) : canCullToTheRight_(canCullToTheRight) {}
  bool ClipLine(Point p0, Point p1, const ClipRect& clip);
  bool ClipQuad(const Point pts[3], const ClipRect& clip);
  bool ClipCubic(const Point pts[4], const ClipRect& clip);
  // The next clipped verb (kLine/kQuad/kCubic), or -1 when the segment is done.
  int Next(Point pts[4]);
  bool CanCullToTheRight() const { return canCullToTheRight_; }
  void AppendLine(Point p0, Point p1);
  void AppendVLine(float x, float y0, float y1, bool reverse);
  void AppendQuad(const Point pts[3], bool reverse);
  void AppendCubic(const Point pts[4], bool reverse);

 private:
  void ClipMonoQuad(const Point srcPts[3], const ClipRect& clip);
  void ClipMonoCubic(const Point srcPts[4], const ClipRect& clip);
  void Reset();
  // kMaxVerbs: the largest split (9 pieces of line + cubic); kMaxPoints: their
  // points, plus the two line points per piece.
  enum { kMaxVerbs = 18, kMaxPoints = 54 };
  Point points_[kMaxPoints];
  uint8_t verbs_[kMaxVerbs];
  int pointCount_ = 0;
  int verbCount_ = 0;
  int verbStop_ = 0;
  int point_ = 0;
  int verb_ = 0;
  const bool canCullToTheRight_;
};

// The chopping helpers the clip stages share: the y cut at a clip boundary and
// the monotone axis intersections (SkBezierCubic, here by bisection).
void ClampLe(float* value, float max);
void ClampGe(float* value, float min);
bool SortIncreasingY(Point dst[], const Point src[], int count);
float BisectQuadAt(float c0, float c1, float c2, float target);
float BisectCubicAt(float c0, float c1, float c2, float c3, float target);
void ChopQuadInY(Point pts[3], const ClipRect& clip);
void ChopCubicInY(Point pts[4], const ClipRect& clip);
void ChopMonoCubicAtY(Point src[4], float y, Point dst[7]);
void ChopMonoCubicAtX(Point src[4], float x, Point dst[7]);

// SkGeometry's chop helpers the clipper needs on top of the y-extrema ones.
void ChopQuadAt(const Point src[3], Point dst[5], float t);
void ChopCubicAt(const Point src[4], Point dst[7], float t);
int ChopQuadAtXExtrema(const Point src[3], Point dst[5]);
int ChopCubicAtXExtrema(const Point src[4], Point dst[10]);
}  // namespace ruyijs::web_canvas::raster
#endif
