// The fill operations that write pixels: fill(path) and fillRect(rect) rasterize
// through the analytic scan converter into the canvas byte store, which is the
// premultiplied surface Firefox keeps (getImageData and toDataURL convert at
// their boundary).
#include <cmath>
#include <string>
#include <vector>

#include "web_canvas/internal.h"
#include "web_canvas/raster_color.h"
#include "web_canvas/raster_commands.h"

namespace ruyijs::web_canvas {
namespace {
bool StateString(JSContext* cx, JS::HandleObject context, int32_t field, std::string* out) {
  JS::RootedObject drawing(cx, detail::DrawingState(cx, context));
  if (!drawing) return false;
  JS::RootedValue value(cx);
  if (!JS_GetElement(cx, drawing, uint32_t(field), &value)) return false;
  if (!value.isString()) return true;
  JS::RootedString text(cx, value.toString());
  JS::UniqueChars utf8 = JS_EncodeStringToUTF8(cx, text);
  if (!utf8) return false;
  *out = utf8.get();
  return true;
}

bool StateNumber(JSContext* cx, JS::HandleObject context, int32_t field, double* out) {
  JS::RootedObject drawing(cx, detail::DrawingState(cx, context));
  if (!drawing) return false;
  JS::RootedValue value(cx);
  if (!JS_GetElement(cx, drawing, uint32_t(field), &value)) return false;
  *out = value.isNumber() ? value.toNumber() : 1.0;
  return true;
}

// The premultiplied surface of the canvas this context draws into.
bool Target(State& state, JS::HandleObject context, raster::Surface* surface,
            JS::MutableHandleObject bitmapOut) {
  CanvasBinding* binding = detail::BindingOf(state, context);
  JSObject* bitmap = detail::BitmapOf(binding);
  if (!binding || !bitmap) return false;
  uint8_t* pixels = detail::ViewBytes(bitmap);
  if (!pixels) return false;
  surface->pixels = pixels;
  surface->width = int(binding->width);
  surface->height = int(binding->height);
  bitmapOut.set(bitmap);
  return true;
}

// True on success. |usable| is false when the fillStyle has no CSS colour, which
// paints nothing rather than failing.
bool CurrentPaint(JSContext* cx, JS::HandleObject context, raster::Paint* paint, bool* usable) {
  *usable = false;
  std::string style, composite = "source-over";
  double alpha = 1.0;
  if (!StateString(cx, context, kFillStyle, &style) ||
      !StateString(cx, context, kGlobalCompositeOperation, &composite) ||
      !StateNumber(cx, context, kGlobalAlpha, &alpha)) {
    return false;
  }
  *usable = raster::PaintFromStyle(style, alpha, composite, paint);
  return true;
}

// The recorded path of a context or Path2D, in the flat command form.
bool PathCommands(JSContext* cx, JS::HandleObject owner, std::vector<double>* commands) {
  JS::RootedObject path(cx, &JS::GetReservedSlot(owner, kContextPathSlot).toObject());
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, path, &length)) return false;
  commands->reserve(length);
  for (uint32_t index = 0; index < length; ++index) {
    JS::RootedValue value(cx);
    if (!JS_GetElement(cx, path, index, &value)) return false;
    commands->push_back(value.toNumber());
  }
  return true;
}
}  // namespace

// fill() / fill(path, rule): the recorded path is rasterized with the current
// fillStyle, globalAlpha and composite operation.
bool FillRecordedPath(State& state, JS::HandleObject context, JS::HandleObject pathOwner,
                      bool evenOdd) {
  JSContext* cx = state.cx;
  raster::Surface surface;
  JS::RootedObject bitmap(cx);
  if (!Target(state, context, &surface, &bitmap)) return true;
  raster::Paint paint;
  bool usable = false;
  if (!CurrentPaint(cx, context, &paint, &usable)) return false;
  if (!usable) return true;
  std::vector<double> commands;
  if (!PathCommands(cx, pathOwner, &commands)) return false;
  raster::Path path;
  raster::BuildPathFromCommands(commands.data(), uint32_t(commands.size()), evenOdd, &path);
  if (path.points.empty()) return true;
  raster::FillPath(path, surface, paint);
  return true;
}

// fillRect(): Skia draws an integer-aligned rectangle without anti-aliasing;
// anything else goes through the same path filler as a rectangle path.
bool FillRecordedRect(State& state, JS::HandleObject context, const double rect[4]) {
  JSContext* cx = state.cx;
  if (!(rect[2] > 0.0) || !(rect[3] > 0.0)) return true;
  raster::Surface surface;
  JS::RootedObject bitmap(cx);
  if (!Target(state, context, &surface, &bitmap)) return true;
  raster::Paint paint;
  bool usable = false;
  if (!CurrentPaint(cx, context, &paint, &usable)) return false;
  if (!usable) return true;
  double matrix[6];
  if (!detail::GetMatrix(cx, context, matrix)) return false;
  const bool axisAligned = matrix[1] == 0.0 && matrix[2] == 0.0;
  const double left = matrix[0] * rect[0] + matrix[4];
  const double top = matrix[3] * rect[1] + matrix[5];
  const double right = left + matrix[0] * rect[2];
  const double bottom = top + matrix[3] * rect[3];
  const bool integral = axisAligned && left == std::floor(left) && top == std::floor(top) &&
                        right == std::floor(right) && bottom == std::floor(bottom);
  if (integral) {
    raster::Path path;
    path.Move(raster::Point{float(left), float(top)});
    path.Line(raster::Point{float(right), float(top)});
    path.Line(raster::Point{float(right), float(bottom)});
    path.Line(raster::Point{float(left), float(bottom)});
    path.Close();
    // A rectangle on whole pixels keeps the rasterizer away: every covered pixel
    // is fully covered, so the blit is the same one Skia's non-AA rect takes.
    raster::FillPath(path, surface, paint);
    return true;
  }
  raster::Path path;
  path.Move(raster::Point{float(matrix[0] * rect[0] + matrix[2] * rect[1] + matrix[4]),
                          float(matrix[1] * rect[0] + matrix[3] * rect[1] + matrix[5])});
  path.Line(raster::Point{float(matrix[0] * (rect[0] + rect[2]) + matrix[2] * rect[1] + matrix[4]),
                          float(matrix[1] * (rect[0] + rect[2]) + matrix[3] * rect[1] + matrix[5])});
  path.Line(raster::Point{float(matrix[0] * (rect[0] + rect[2]) + matrix[2] * (rect[1] + rect[3]) +
                                matrix[4]),
                          float(matrix[1] * (rect[0] + rect[2]) + matrix[3] * (rect[1] + rect[3]) +
                                matrix[5])});
  path.Line(raster::Point{float(matrix[0] * rect[0] + matrix[2] * (rect[1] + rect[3]) + matrix[4]),
                          float(matrix[1] * rect[0] + matrix[3] * (rect[1] + rect[3]) + matrix[5])});
  path.Close();
  raster::FillPath(path, surface, paint);
  return true;
}
}  // namespace ruyijs::web_canvas
