// SkLineClipper.cpp and the buffer half of SkEdgeClipper: the clipped segment
// list Skia builds before it constructs edges, plus the line clip that turns the
// parts outside the clip into vertical edges on the boundary.
#include <algorithm>
#include <cmath>
#include <cstring>

#include "web_canvas/raster_clip.h"

namespace ruyijs::web_canvas::raster {
namespace {
template <typename T>
T PinUnsorted(T value, T limit0, T limit1) {
  if (limit1 < limit0) std::swap(limit0, limit1);
  if (value < limit0) return limit0;
  if (value > limit1) return limit1;
  return value;
}

// sect_with_horizontal: the x where the segment crosses y = Y.
float SectWithHorizontal(const Point src[2], float y) {
  const float dy = src[1].y - src[0].y;
  if (std::abs(dy) < 1e-6f) return (src[0].x + src[1].x) / 2;
  const double x0 = src[0].x, y0 = src[0].y, x1 = src[1].x, y1 = src[1].y;
  const double result = x0 + (double(y) - y0) * (x1 - x0) / (y1 - y0);
  return float(PinUnsorted(result, x0, x1));
}

// sect_with_vertical: the y where the segment crosses x = X.
float SectWithVertical(const Point src[2], float x) {
  const float dx = src[1].x - src[0].x;
  if (std::abs(dx) < 1e-6f) return (src[0].y + src[1].y) / 2;
  const double x0 = src[0].x, y0 = src[0].y, x1 = src[1].x, y1 = src[1].y;
  const double result = y0 + (double(x) - x0) * (y1 - y0) / (x1 - x0);
  return float(result);
}

float SectClampWithVertical(const Point src[2], float x) {
  return PinUnsorted(SectWithVertical(src, x), src[0].y, src[1].y);
}

}  // namespace

int ClipLineSegments(const Point pts[2], const ClipRect& clip, Point lines[kMaxClippedPoints],
                     bool canCullToTheRight) {
  int index0, index1;
  if (pts[0].y < pts[1].y) {
    index0 = 0;
    index1 = 1;
  } else {
    index0 = 1;
    index1 = 0;
  }
  if (pts[index1].y <= clip.top) return 0;     // above the clip
  if (pts[index0].y >= clip.bottom) return 0;  // below the clip

  Point tmp[2];
  std::memcpy(tmp, pts, sizeof(tmp));
  if (pts[index0].y < clip.top) tmp[index0] = Point{SectWithHorizontal(pts, clip.top), clip.top};
  if (tmp[index1].y > clip.bottom) {
    tmp[index1] = Point{SectWithHorizontal(pts, clip.bottom), clip.bottom};
  }

  Point resultStorage[kMaxClippedPoints];
  Point* result = tmp;
  int lineCount = 1;
  bool reverse;
  if (pts[0].x < pts[1].x) {
    index0 = 0;
    index1 = 1;
    reverse = false;
  } else {
    index0 = 1;
    index1 = 0;
    reverse = true;
  }
  if (tmp[index1].x <= clip.left) {  // wholly to the left
    tmp[0].x = tmp[1].x = clip.left;
    result = tmp;
    reverse = false;
  } else if (tmp[index0].x >= clip.right) {  // wholly to the right
    if (canCullToTheRight) return 0;
    tmp[0].x = tmp[1].x = clip.right;
    result = tmp;
    reverse = false;
  } else {
    result = resultStorage;
    Point* r = result;
    if (tmp[index0].x < clip.left) {
      r->x = clip.left;
      r->y = tmp[index0].y;
      ++r;
      r->x = clip.left;
      r->y = SectClampWithVertical(tmp, clip.left);
    } else {
      *r = tmp[index0];
    }
    ++r;
    if (tmp[index1].x > clip.right) {
      r->x = clip.right;
      r->y = SectClampWithVertical(tmp, clip.right);
      ++r;
      r->x = clip.right;
      r->y = tmp[index1].y;
    } else {
      *r = tmp[index1];
    }
    lineCount = int(r - result);
  }
  if (reverse) {
    for (int i = 0; i <= lineCount; ++i) lines[lineCount - i] = result[i];
  } else {
    std::memcpy(lines, result, size_t(lineCount + 1) * sizeof(Point));
  }
  return lineCount;
}

bool EdgeClipper::ClipLine(Point p0, Point p1, const ClipRect& clip) {
  Reset();
  const Point pts[2] = {p0, p1};
  Point lines[kMaxClippedPoints];
  const int count = ClipLineSegments(pts, clip, lines, CanCullToTheRight());
  for (int i = 0; i < count; ++i) AppendLine(lines[i], lines[i + 1]);
  verbStop_ = verbCount_;
  return verbStop_ != 0;
}

void EdgeClipper::Reset() {
  pointCount_ = 0;
  verbCount_ = 0;
  verbStop_ = 0;
  point_ = 0;
  verb_ = 0;
}

void EdgeClipper::AppendLine(Point p0, Point p1) {
  verbs_[verbCount_++] = kLine;
  points_[pointCount_++] = p0;
  points_[pointCount_++] = p1;
}

void EdgeClipper::AppendVLine(float x, float y0, float y1, bool reverse) {
  verbs_[verbCount_++] = kLine;
  points_[pointCount_++] = reverse ? Point{x, y1} : Point{x, y0};
  points_[pointCount_++] = reverse ? Point{x, y0} : Point{x, y1};
}

void EdgeClipper::AppendQuad(const Point pts[3], bool reverse) {
  verbs_[verbCount_++] = kQuad;
  for (int i = 0; i < 3; ++i) points_[pointCount_++] = reverse ? pts[2 - i] : pts[i];
}

void EdgeClipper::AppendCubic(const Point pts[4], bool reverse) {
  verbs_[verbCount_++] = kCubic;
  for (int i = 0; i < 4; ++i) points_[pointCount_++] = reverse ? pts[3 - i] : pts[i];
}

int EdgeClipper::Next(Point pts[4]) {
  if (verb_ >= verbStop_) return -1;
  const uint8_t verb = verbs_[verb_++];
  const int count = verb == kLine ? 2 : verb == kQuad ? 3 : 4;
  for (int i = 0; i < count; ++i) pts[i] = points_[point_ + i];
  point_ += count;
  return verb;
}
}  // namespace ruyijs::web_canvas::raster
