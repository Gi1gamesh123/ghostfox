// The remaining public ruyijs methods. Drawing has no rasterizer (D35).
#include <cmath>
#include <cstring>
#include "web_canvas/internal.h"
#include "document_model/state.h"
#include "web_media/image_codec.h"
#include "js/ForOfIterator.h"
#include "js/Realm.h"

namespace ruyijs::web_canvas {
namespace {
struct DrawPixels { uint32_t width = 0, height = 0; std::vector<uint8_t> bytes; };

bool ReadDrawPixels(State& state, JS::HandleValue value, DrawPixels* out) {
  if (!value.isObject()) return false;
  JS::RootedObject object(state.cx, js::CheckedUnwrapStatic(&value.toObject()));
  if (!object) return false;
  if (JS::GetClass(object) == &kBitmapClass) {
    out->width = uint32_t(JS::GetReservedSlot(object, 0).toNumber());
    out->height = uint32_t(JS::GetReservedSlot(object, 1).toNumber());
    JS::Value slot = JS::GetReservedSlot(object, 2);
    if (!slot.isObject()) return false;
    JSObject* view = &slot.toObject();
    out->bytes.resize(JS_GetArrayBufferViewByteLength(view));
    if (!out->bytes.empty()) std::memcpy(out->bytes.data(), detail::ViewBytes(view), out->bytes.size());
    return out->width && out->height && out->bytes.size() >= size_t(out->width) * out->height * 4;
  }
  if (JS::GetClass(object) == &document_model::kNodeClass) {
    auto* wrapper = static_cast<document_model::Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate());
    if (!wrapper || wrapper->dead || wrapper->imageBytes.empty() || wrapper->imageMime.empty()) return false;
    ruyijs::domain::web_media::ImageData decoded;
    std::vector<uint8_t> encoded(wrapper->imageBytes.begin(), wrapper->imageBytes.end());
    const auto status = web_media::DecodeImageBytes(wrapper->imageMime, encoded,
                                                     wrapper->imageNaturalWidth,
                                                     wrapper->imageNaturalHeight, false, &decoded);
    if (status != ruyijs::domain::web_media::ImageDecodeStatus::Ok || decoded.frames.empty()) return false;
    const auto& frame = decoded.frames.front();
    out->width = frame.width; out->height = frame.height; out->bytes = frame.pixels;
    return out->width && out->height && out->bytes.size() >= size_t(out->width) * out->height * 4;
  }
  return false;
}

bool DrawImagePixels(State& state, JS::HandleObject context, JS::HandleValue image,
                     const double* values, unsigned count) {
  DrawPixels source;
  if (!ReadDrawPixels(state, image, &source)) return true;
  CanvasBinding* binding = detail::BindingOf(state, context);
  JSObject* target = detail::BitmapOf(binding);
  if (!binding || !target) return true;
  uint8_t* dst = detail::ViewBytes(target);
  if (!dst) return true;
  double sx = 0, sy = 0, sw = source.width, sh = source.height;
  double dx = 0, dy = 0, dw = source.width, dh = source.height;
  if (count == 2) { dx = values[0]; dy = values[1]; }
  else if (count == 4) { dx = values[0]; dy = values[1]; dw = values[2]; dh = values[3]; }
  else { sx = values[0]; sy = values[1]; sw = values[2]; sh = values[3]; dx = values[4]; dy = values[5]; dw = values[6]; dh = values[7]; }
  if (!(sw > 0 && sh > 0 && dw > 0 && dh > 0)) return true;
  const int x0 = std::max(0, int(std::floor(dx))), y0 = std::max(0, int(std::floor(dy)));
  const int x1 = std::min(int(binding->width), int(std::ceil(dx + dw)));
  const int y1 = std::min(int(binding->height), int(std::ceil(dy + dh)));
  for (int y = y0; y < y1; ++y) for (int x = x0; x < x1; ++x) {
    const int tx = std::clamp(int((x - dx) * source.width / dw + sx), 0, int(source.width) - 1);
    const int ty = std::clamp(int((y - dy) * source.height / dh + sy), 0, int(source.height) - 1);
    std::memcpy(dst + (size_t(y) * binding->width + x) * 4,
                source.bytes.data() + (size_t(ty) * source.width + tx) * 4, 4);
  }
  return true;
}
} // namespace
namespace detail {
bool ResetContext2D(State& s, JS::HandleObject self) {
  JSContext* cx = s.cx;
  JS::RootedObject drawing(cx), stack(cx, JS::NewArrayObject(cx, 0));
  JS::RootedObject path(cx, JS::NewArrayObject(cx, 0));
  if (!stack || !path || !NewDrawingState(cx, &drawing)) return false;
  CanvasBinding* binding = BindingOf(s, self);
  if (binding && !ResizeBitmap(s, *binding, binding->width, binding->height)) return false;
  JS::SetReservedSlot(self, kContextStateSlot, JS::ObjectValue(*drawing));
  JS::SetReservedSlot(self, kContextStackSlot, JS::ObjectValue(*stack));
  JS::SetReservedSlot(self, kContextPathSlot, JS::ObjectValue(*path));
  return true;
}
}  // namespace detail
bool Context2DExtra(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *detail::Self(args);
  int op = detail::ContextSlot(args);
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  args.rval().setUndefined();
  if (op == 0 || op == 1) { args.rval().setBoolean(false); return true; }
  if (op == 2) return detail::ResetContext2D(s, self);
  if (op == 3 || op == 4) {
    JS::RootedObject drawing(cx, detail::DrawingState(cx, self));
    JS::RootedObject dash(cx, JS::NewArrayObject(cx, 0));
    if (!drawing || !dash) return false;
    JS::RootedValue value(cx);
    uint32_t n = 0;
    if (op == 3) {
      if (!argc) return WebKit::Fail(cx, "setLineDash requires a sequence");
      JS::ForOfIterator it(cx);
      if (!it.init(args[0])) return false;
      bool done = false, valid = true;
      while (true) {
        if (!it.next(&value, &done)) return false;
        if (done) break;
        double number;
        if (!JS::ToNumber(cx, value, &number)) return false;
        valid &= std::isfinite(number) && number >= 0;
        if (n >= 8192) return WebKit::Fail(cx, "Line dash limit exceeded");
        value.setNumber(number);
        if (!JS_SetElement(cx, dash, n++, value)) return false;
      }
      if (!valid) return true;
      if (n % 2) for (uint32_t i = 0; i < n; ++i) {
        if (!JS_GetElement(cx, dash, i, &value) || !JS_SetElement(cx, dash, n+i, value)) return false;
      }
      value.setObject(*dash);
      return JS_SetElement(cx, drawing, kLineDash, value);
    }
    if (!JS_GetElement(cx, drawing, kLineDash, &value)) return false;
    JS::RootedObject saved(cx, &value.toObject());
    if (!JS::GetArrayLength(cx, saved, &n)) return false;
    for (uint32_t i = 0; i < n; ++i)
      if (!JS_GetElement(cx, saved, i, &value) || !JS_SetElement(cx, dash, i, value)) return false;
    args.rval().setObject(*dash);
    return true;
  }
  // stroke/clip validate optional Path2D and fill rule; no pixels are painted.
  if (op == 5 || op == 6) {
    unsigned rule = 0;
    if (args.get(0).isObject() && JS::GetClass(&args[0].toObject()) == &kPath2DClass) rule = 1;
    if (op == 6 && !args.get(rule).isUndefined()) {
      JS::RootedString text(cx, JS::ToString(cx, args.get(rule)));
      bool a, b;
      if (!text || !detail::StringEqualsAscii(cx, text, "nonzero", &a) ||
          !detail::StringEqualsAscii(cx, text, "evenodd", &b)) return false;
      if (!a && !b) return WebKit::Fail(cx, "Invalid CanvasFillRule");
    }
    return true;
  }
  if (op == 12) return true; // Offscreen commit, no presentation device.
  unsigned first = op == 8 || op == 9 ? 1 : 0;
  unsigned count = op == 7 || op == 11 ? 4 : op == 10 ? 5 : 2;
  if (argc < first+count) return WebKit::Fail(cx, "Missing drawing arguments");
  if (op == 8 && !JS::ToString(cx, args[0])) return false;
  if (op == 9 && !args[0].isObject()) return WebKit::Fail(cx, "drawImage requires an image");
  double values[8]; bool usable;
  if (op == 9) {
    if (argc != 3 && argc != 5 && argc != 9) return WebKit::Fail(cx, "Invalid drawImage overload");
    count = argc-1;
  } else if (op == 8 && argc > 3) count = 3;
  if (!detail::ReadNumbers(cx, args, first, count, values, &usable)) return false;
  if (op == 9 && usable && !DrawImagePixels(s, self, args[0], values, count)) return false;
  if (op == 10 && usable && values[4] < 0)
    return ThrowDOMException(s, "IndexSizeError", "Negative radius");
  return true;
}
bool InstallContext2DExtra(State& s, WebFace& face) {
  const char* names[] = {"isContextLost","isPointInStroke","reset","setLineDash","getLineDash",
    "stroke","clip","strokeRect","strokeText","drawImage","arcTo","roundRect"};
  const unsigned lengths[] = {0,2,0,1,0,0,0,4,3,3,5,4};
  for (int i = 0; i < 11; ++i)
    if (!detail::Method2D(s, face, names[i], Context2DExtra, lengths[i], i)) return false;
  const char* resources[] = {"createLinearGradient","createRadialGradient","createConicGradient","createPattern"};
  const unsigned counts[] = {4,6,3,2};
  for (int i = 0; i < 4; ++i)
    if (!detail::Method2D(s, face, resources[i], CreateCanvasResource, counts[i], i)) return false;
  return detail::Method2D(s, face, "measureText", MeasureText, 1, 0) &&
         detail::Method2D(s, face, "roundRect", RoundRect, 4, 0);
}
bool InstallOffscreenContext(State& s) {
  WebFace& f = s.offscreen2d;
  const char* name = "OffscreenCanvasRenderingContext2D";
  JS::RootedObject parent(s.cx, JS::GetRealmObjectPrototype(s.cx));
  return s.kit.Define(name, &kOffscreen2DClass, parent, WebKit::Reject, &f) &&
         InstallContextMembers(s, f) &&
         detail::Method2D(s, f, "commit", Context2DExtra, 0, 12);
}
}
