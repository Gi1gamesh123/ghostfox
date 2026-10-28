#include "web_canvas/internal.h"
#include "js/CallAndConstruct.h"
#include "js/ErrorReport.h"
#include "js/Exception.h"
#include "js/Conversions.h"
#include "js/Wrapper.h"

namespace ruyijs::web_canvas {
namespace {
bool Transfer(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *detail::Self(args);
  int op = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (op == 2) {
    if (!WebKit::Check(cx, args.thisv(), &kOffscreenClass, &self, "Illegal invocation")) return false;
    JS::RootedObject bitmap(cx);
    if (!NewCanvasBitmap(s, self, &bitmap)) return false;
    CanvasBinding* b = s.Find(self);
    if (!detail::ResizeBitmap(s, *b, b->width, b->height)) return false;
    args.rval().setObject(*bitmap);
    return true;
  }
  if (!detail::CanvasReceiver(s, args.thisv(), &self, &args)) return false;
  if (op == 0) {
    // captureStream([frameRequestRate]): the Media owner's CanvasCaptureMediaStream.
    if (args.length() > 0 && !args[0].isUndefined()) {
      double rate = 0;
      if (!JS::ToNumber(cx, args[0], &rate)) return false;
    }
    if (!s.capturePort) return WebKit::Fail(cx, "captureStream requires the media owner");
    JS::RootedObject stream(cx);
    if (!s.capturePort(s.capturePortContext, self, &stream)) return false;
    JS::RootedValue value(cx, JS::ObjectValue(*stream));
    if (!JS_WrapValue(cx, &value)) return false;
    args.rval().set(value);
    return true;
  }
  uint32_t width, height;
  if (!detail::CanvasDimensions(s, self, &width, &height)) return false;
  JS::RootedValue ctor(cx, JS::ObjectValue(*s.offscreen.constructor));
  JS::RootedValueArray<2> values(cx);
  values[0].setNumber(width); values[1].setNumber(height);
  JS::RootedObject result(cx);
  if (!JS::Construct(cx, ctor, values, &result)) return false;
  args.rval().setObject(*result);
  return true;
}
}
// createImageBitmap on its global holder (Window, or WorkerGlobalScope.prototype)
// and the OffscreenCanvas/ImageData members shared by Window and Worker realms.
bool InstallCanvasTransfersOn(State& s, JS::HandleObject holder, const char* iface) {
  return detail::MethodOn(s.kit, holder, iface, "createImageBitmap", CreateImageBitmap, 1, 0) &&
    s.kit.Method(s.offscreen, "OffscreenCanvas", "transferToImageBitmap", Transfer, 0, 2) &&
    JS_DefineProperty(s.cx, s.offscreen.constructor, "length", 2, JSPROP_READONLY) &&
    JS_DefineProperty(s.cx, s.imageData.constructor, "length", 2, JSPROP_READONLY);
}
bool InstallCanvasTransfers(State& s) {
  JS::RootedObject page(s.cx, s.kit.page);
  return InstallCanvasTransfersOn(s, page, "Window") &&
    detail::MethodOn(s.kit, s.canvasPrototype, "HTMLCanvasElement", "captureStream", Transfer, 0, 0) &&
    detail::MethodOn(s.kit, s.canvasPrototype, "HTMLCanvasElement", "transferControlToOffscreen", Transfer, 0, 1);
}
}
