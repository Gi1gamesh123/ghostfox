#include "web_canvas/raster_path.h"

namespace ruyijs::web_canvas::raster {
// PathBuilderSkia::LineTo starts the path with a moveTo when it is still empty;
// it does not add a line from the origin.

void Path::Move(Point p) {
  if (!verbs.empty() && verbs.back() == kMove) {
    points.back() = p;
    return;
  }
  points.push_back(p);
  verbs.push_back(kMove);
}

void Path::Line(Point p) {
  if (points.empty()) {
    Move(p);
    return;
  }
  points.push_back(p);
  verbs.push_back(kLine);
}

void Path::Quad(Point p1, Point p2) {
  if (points.empty()) Move(p1);
  points.push_back(p1);
  points.push_back(p2);
  verbs.push_back(kQuad);
}

void Path::Cubic(Point p1, Point p2, Point p3) {
  if (points.empty()) Move(p1);
  points.push_back(p1);
  points.push_back(p2);
  points.push_back(p3);
  verbs.push_back(kCubic);
}

// SkPathBuilder::close() ignores a second close in a row.
void Path::Close() {
  if (verbs.empty() || verbs.back() == kClose) return;
  verbs.push_back(kClose);
}

// The start of the first contour: its moveTo, or the implicit (0,0).
Point Path::BeginPoint() const {
  return !verbs.empty() && verbs[0] == kMove ? points[0] : Point{};
}
}  // namespace ruyijs::web_canvas::raster
