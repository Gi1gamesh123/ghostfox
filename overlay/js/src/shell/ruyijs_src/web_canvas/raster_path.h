#ifndef RUYIJS_WEB_CANVAS_RASTER_PATH_H
#define RUYIJS_WEB_CANVAS_RASTER_PATH_H
// The path the rasterizer consumes: the same verbs Skia receives from
// gfx/2d's PathBuilderSkia, with float32 coordinates (gfx::Float), plus the fill
// rule of the fill call and the canvas byte store it is rasterized into.
#include <cstdint>
#include <vector>

namespace ruyijs::web_canvas::raster {
struct Point {
  float x = 0.0f;
  float y = 0.0f;
};

enum Verb : uint8_t { kMove = 0, kLine, kQuad, kCubic, kClose };

struct Path {
  std::vector<Point> points;
  std::vector<uint8_t> verbs;
  bool evenOdd = false;
  // PathBuilderSkia starts with a moveTo when the first segment is drawn.
  void Move(Point p);
  void Line(Point p);
  void Quad(Point p1, Point p2);
  void Cubic(Point p1, Point p2, Point p3);
  void Close();
  bool Empty() const { return points.empty(); }
  Point CurrentPoint() const;
  Point BeginPoint() const;
};

// The premultiplied RGBA8 byte store of one canvas (Firefox keeps the same
// channels premultiplied and in B8G8R8A8 order; only the byte order differs).
struct Surface {
  uint8_t* pixels = nullptr;
  int width = 0;
  int height = 0;
};

// The Skia blend modes this rasterizer implements, by the canvas
// globalCompositeOperation spelling that selects them.
enum class Blend : int {
  kSourceOver = 0, kSourceIn, kSourceOut, kSourceATop, kDestinationOver,
  kDestinationIn, kDestinationOut, kDestinationATop, kXor, kPlus, kCopy,
  kDestination, kMultiply, kScreen, kOverlay, kDarken, kLighten, kDifference,
  kExclusion, kHardLight,
};
// The paint of one fill: the premultiplied source colour bytes and the blend.
struct Paint {
  uint8_t rgba[4] = {0, 0, 0, 0};
  Blend blend = Blend::kSourceOver;
};

// raster_fill.cpp: fills |path| into |surface| the way DrawTargetSkia does.
void FillPath(const Path& path, Surface surface, const Paint& paint);
}  // namespace ruyijs::web_canvas::raster
#endif
