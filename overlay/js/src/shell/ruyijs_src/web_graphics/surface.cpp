// The handoff from the WebGL drawing buffer to the canvas pixel store. The
// Canvas owner keeps one RGBA byte store per canvas and encodes it in
// HTMLCanvasElement.toDataURL(); a WebGL context has no store of its own, so the
// resolve writes the drawing buffer into that store through the canvas's own 2D
// pixel interface. The encoder, its filtering and the deBG chunk stay with the
// Canvas owner, which is why this file only moves bytes.
#include <algorithm>
#include <cstring>

#include "js/PropertyAndElement.h"
#include "js/ValueArray.h"
#include "js/experimental/TypedData.h"
#include "web_graphics/draw_calls.h"
#include "web_graphics/internal.h"

namespace ruyijs::web_graphics {
namespace {
bool CallMethod(JSContext* cx, JS::HandleObject self, const char* name,
                const JS::HandleValueArray& arguments, JS::MutableHandleValue out) {
  JS::RootedValue function(cx);
  if (!JS_GetProperty(cx, self, name, &function) || !function.isObject()) return false;
  return JS_CallFunctionValue(cx, self, function, arguments, out);
}

// The canvas's 2D context, kept on the WebGL Context so every draw hands the
// buffer over without rebuilding it.
bool EnsureSurface(JSContext* cx, Context& context, JS::HandleObject canvas, int32_t width,
                   int32_t height, JS::MutableHandleObject surface, bool* created) {
  *created = false;
  if (context.surfaceContext && context.surfaceImage && context.surfacePixels &&
      context.surfaceWidth == width && context.surfaceHeight == height) {
    surface.set(context.surfaceContext);
    return true;
  }
  JS::RootedString twoD(cx, JS_NewStringCopyZ(cx, "2d"));
  if (!twoD) return false;
  JS::RootedValue argument(cx, JS::StringValue(twoD));
  JS::RootedValueArray<1> contextArguments(cx);
  contextArguments[0].set(argument);
  JS::RootedValue result(cx);
  if (!CallMethod(cx, canvas, "getContext", contextArguments, &result)) return false;
  if (!result.isObject()) return true;
  JS::RootedObject context2d(cx, &result.toObject());
  JS::RootedValueArray<2> sizeArguments(cx);
  sizeArguments[0].setInt32(width);
  sizeArguments[1].setInt32(height);
  JS::RootedValue image(cx);
  if (!CallMethod(cx, context2d, "createImageData", sizeArguments, &image)) return false;
  if (!image.isObject()) return true;
  JS::RootedObject imageObject(cx, &image.toObject());
  JS::RootedValue pixels(cx);
  if (!JS_GetProperty(cx, imageObject, "data", &pixels) || !pixels.isObject()) return true;
  context.surfaceContext = context2d;
  context.surfaceImage = imageObject;
  context.surfacePixels = &pixels.toObject();
  context.surfaceWidth = width;
  context.surfaceHeight = height;
  surface.set(context2d);
  *created = true;
  return true;
}
}  // namespace

bool SyncCanvasSurface(State& state, Context& context) {
  JSContext* cx = state.cx;
  int32_t width = 300, height = 150;
  draw_call_detail::CanvasSize(cx, context, &width, &height);
  if (width <= 0 || height <= 0 || !context.canvas) return true;
  if (context.drawingWidth != width || context.drawingHeight != height) return true;
  JS::RootedObject canvas(cx, context.canvas);
  JS::RootedObject surface(cx);
  bool created = false;
  if (!EnsureSurface(cx, context, canvas, width, height, &surface, &created)) return true;
  if (!surface) return true;
  // The store is premultiplied like every canvas store; ImageData is straight
  // alpha, so the resolve is undone here and putImageData premultiplies it back.
  // The drawing buffer's row 0 is the bottom of the image, the store's row 0 the
  // top, so the rows are handed over in reverse.
  {
    JS::AutoCheckCannotGC nogc;
    bool shared = false;
    uint8_t* target = static_cast<uint8_t*>(
        JS_GetArrayBufferViewData(context.surfacePixels, &shared, nogc));
    const size_t length = JS_GetTypedArrayByteLength(context.surfacePixels);
    const uint8_t* source = context.drawingBuffer.data();
    const size_t stride = size_t(width) * 4u;
    if (target && source && length >= stride * size_t(height))
      for (int32_t row = 0; row < height; ++row) {
        const uint8_t* from = source + stride * size_t(height - 1 - row);
        uint8_t* to = target + stride * size_t(row);
        for (size_t index = 0; index + 4u <= stride; index += 4u) {
          const unsigned alpha = from[index + 3u];
          for (size_t channel = 0; channel < 3; ++channel) {
            const unsigned value = from[index + channel];
            to[index + channel] = uint8_t(alpha == 0 ? 0
                : std::min(255u, (value * 255u + alpha / 2u) / alpha));
          }
          to[index + 3u] = uint8_t(alpha);
        }
      }
  }
  JS::RootedValueArray<3> arguments(cx);
  arguments[0].setObject(*context.surfaceImage);
  arguments[1].setInt32(0);
  arguments[2].setInt32(0);
  JS::RootedValue ignored(cx);
  return CallMethod(cx, surface, "putImageData", arguments, &ignored);
}
}  // namespace ruyijs::web_graphics
