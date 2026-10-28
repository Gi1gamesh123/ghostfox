// gfx/2d/PathHelpers.h ArcToBezier and the canvas command list: the recorded
// path verbs become the float32 path Skia receives. Coordinates are transformed
// by the matrix recorded with each command, which is the device-space path
// Firefox keeps (CanvasRenderingContext2D::FlushPathTransform).
#include <cmath>

#include "web_canvas/raster_commands.h"

namespace ruyijs::web_canvas::raster {
namespace {
// gfx::Matrix in the [a b c d e f] order the canvas records.
struct Transform {
  double a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;
  Point Apply(double x, double y) const {
    return Point{float(a * x + c * y + e), float(b * x + d * y + f)};
  }
};

constexpr double kPi = 3.14159265358979323846;

// ComputeKappaFactor: (4/3) * tan(angle / 4).
float KappaFactor(float angle) { return (4.0f / 3.0f) * std::tan(angle / 4.0f); }
}  // namespace

// ArcToBezier: a partial arc of at most 90 degrees, given exact end points.
void ArcToBezier(Path* sink, double cx, double cy, double radiusX, double radiusY,
                 float startAngle, float endAngle, bool anticlockwise, float rotation,
                 const double matrix[6]) {
  const Transform outer{matrix[0], matrix[1], matrix[2], matrix[3], matrix[4], matrix[5]};
  auto apply = [&](double x, double y) {
    // The arc's own scaling, the optional rotation, then the canvas transform.
    const double c = std::cos(double(rotation)), s = std::sin(double(rotation));
    double px = radiusX * x, py = radiusY * y;
    if (rotation != 0.0f) {
      const double rx = px * c - py * s, ry = px * s + py * c;
      px = rx;
      py = ry;
    }
    return outer.Apply(px + cx, py + cy);
  };
  const float sweepDirection = anticlockwise ? -1.0f : 1.0f;
  float sweepLeft = (endAngle - startAngle) * sweepDirection;
  if (sweepLeft < 0) {
    sweepLeft = float(2.0 * kPi) + std::fmod(sweepLeft, float(2.0 * kPi));
    startAngle = endAngle - sweepLeft * sweepDirection;
  } else if (sweepLeft > float(2.0 * kPi)) {
    sweepLeft = float(2.0 * kPi);
  }
  float currentStartAngle = startAngle;
  double startOffsetX = std::cos(double(startAngle)), startOffsetY = std::sin(double(startAngle));
  sink->Line(apply(startOffsetX, startOffsetY));
  const float kHalfPi = float(kPi / 2.0);
  while (sweepLeft > 0) {
    const float currentEndAngle = currentStartAngle + (sweepLeft < kHalfPi ? sweepLeft : kHalfPi) *
                                                          sweepDirection;
    const double endOffsetX = std::cos(double(currentEndAngle));
    const double endOffsetY = std::sin(double(currentEndAngle));
    const float kappa = KappaFactor(currentEndAngle - currentStartAngle);
    sink->Cubic(apply(startOffsetX - startOffsetY * kappa, startOffsetY + startOffsetX * kappa),
                apply(endOffsetX + endOffsetY * kappa, endOffsetY - endOffsetX * kappa),
                apply(endOffsetX, endOffsetY));
    sweepLeft -= kHalfPi;
    currentStartAngle = currentEndAngle;
    startOffsetX = endOffsetX;
    startOffsetY = endOffsetY;
  }
}

void BuildPathFromCommands(const double* commands, uint32_t count, bool evenOdd, Path* path) {
  Transform transform;
  uint32_t index = 0;
  while (index < count) {
    double op = commands[index++];
    if (op == 8) {  // A new matrix prefix for the command that follows.
      if (index + 6 > count) return;
      transform = Transform{commands[index], commands[index + 1], commands[index + 2],
                            commands[index + 3], commands[index + 4], commands[index + 5]};
      index += 6;
      if (index >= count) return;
      op = commands[index++];
    }
    auto number = [&](uint32_t offset) { return index + offset < count ? commands[index + offset] : 0.0; };
    switch (int(op)) {
      case 0:  // moveTo
        path->Move(transform.Apply(number(0), number(1)));
        index += 2;
        break;
      case 1:  // lineTo
        path->Line(transform.Apply(number(0), number(1)));
        index += 2;
        break;
      case 2:  // closePath
        path->Close();
        break;
      case 3: {  // rect
        const double x = number(0), y = number(1), w = number(2), h = number(3);
        path->Move(transform.Apply(x, y));
        path->Line(transform.Apply(x + w, y));
        path->Line(transform.Apply(x + w, y + h));
        path->Line(transform.Apply(x, y + h));
        path->Close();
        index += 4;
        break;
      }
      case 4: {  // arc
        const double matrix[6] = {transform.a, transform.b, transform.c,
                                  transform.d, transform.e, transform.f};
        ArcToBezier(path, number(0), number(1), number(2), number(2), float(number(3)),
                    float(number(4)), number(5) != 0.0, 0.0f, matrix);
        index += 6;
        break;
      }
      case 5: {  // ellipse
        const double matrix[6] = {transform.a, transform.b, transform.c,
                                  transform.d, transform.e, transform.f};
        ArcToBezier(path, number(0), number(1), number(2), number(3), float(number(5)),
                    float(number(6)), number(7) != 0.0, float(number(4)), matrix);
        index += 8;
        break;
      }
      case 6:  // quadraticCurveTo
        path->Quad(transform.Apply(number(0), number(1)), transform.Apply(number(2), number(3)));
        index += 4;
        break;
      default:  // bezierCurveTo
        path->Cubic(transform.Apply(number(0), number(1)), transform.Apply(number(2), number(3)),
                    transform.Apply(number(4), number(5)));
        index += 6;
        break;
    }
  }
  path->evenOdd = evenOdd;
}
}  // namespace ruyijs::web_canvas::raster
