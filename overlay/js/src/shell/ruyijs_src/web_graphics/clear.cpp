// Clear the original default RGBA8 drawing buffer; queries and readPixels use
// this same Context state. Scissor/color-mask apply, viewport/blending do not.
#include "web_graphics/internal.h"
#include "web_graphics/draw_calls.h"
#include "web_graphics/draw_sample.h"
#include <algorithm>

namespace ruyijs::web_graphics {
bool ClearCall(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *detail::Self(args);
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  if (!argc) return WebKit::Fail(cx, "WebGL clear requires one argument");
  Context& context = *detail::ContextOf(self);
  uint32_t mask = 0;
  if (!JS::ToUint32(cx, args.get(0), &mask)) return false;
  args.rval().setUndefined();
  if (mask & ~(0x4000u | 0x0100u | 0x0400u)) {
    detail::SetError(context, kInvalidValue);
    return true;
  }
  if (context.drawFramebuffer) { detail::SetError(context, 0x0506); return true; }
  if (!(mask & 0x4000u)) return true;
  int32_t width = 0, height = 0;
  draw_call_detail::CanvasSize(cx, context, &width, &height);
  if (JS_IsExceptionPending(cx)) return false;
  ResizeDrawingBuffer(context, width, height);
  width = context.drawingWidth; height = context.drawingHeight;
  int64_t left = 0, bottom = 0, right = width, top = height;
  ParameterStateValue enabled, box;
  if (GetParameterState(context, 0x0C11, &enabled) && enabled.values[0] &&
      GetParameterState(context, 0x0C10, &box)) {
    left = std::clamp<int64_t>(int64_t(box.values[0]), 0, width);
    bottom = std::clamp<int64_t>(int64_t(box.values[1]), 0, height);
    right = std::clamp<int64_t>(int64_t(box.values[0]) + int64_t(box.values[2]), 0, width);
    top = std::clamp<int64_t>(int64_t(box.values[1]) + int64_t(box.values[3]), 0, height);
  }
  ParameterStateValue color, channels;
  const bool has_color = GetParameterState(context, 0x0C22, &color);
  const bool has_mask = GetParameterState(context, 0x0C23, &channels);
  uint8_t rgba[4] = {};
  for (int channel = 0; channel < 4; ++channel)
    rgba[channel] = uint8_t(draw_detail::Unorm8(has_color ? float(color.values[channel]) : 0.f));
  for (int64_t y = bottom; y < top; ++y) {
    for (int64_t x = left; x < right; ++x) {
      auto* pixel = context.drawingBuffer.data() + (size_t(y) * size_t(width) + size_t(x)) * 4;
      for (int channel = 0; channel < 4; ++channel)
        if (!has_mask || channels.values[channel]) pixel[channel] = rgba[channel];
    }
  }
  return SyncCanvasSurface(state, context);
}
}  // namespace ruyijs::web_graphics
