// CanvasCaptureMediaStream: the MediaStream subclass HTMLCanvasElement.
// captureStream() returns (Firefox 158 shape, artifacts/mouser-datadome/
// iface_shape_probe.js). Instances are this owner's MediaStream objects with
// the source canvas in their StreamCanvas slot, so every MediaStream member
// applies; the canvas owner reaches the factory through its capture port.
// The stream holds one video track from the track factory (track.cpp) whose
// settings follow the canvas size; frames are not produced (DEVIATIONS D135).
#include "web_media/stream.h"
#include "web_media/realm.h"
#include "web_media/track.h"
#include "web_media/media_runtime.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
#include <string>

namespace ruyijs::web_media {
namespace {
bool Receiver(JSContext* cx, const JS::CallArgs& args, const char* member, JS::MutableHandleObject out) {
  out.set(args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (out && JS::GetClass(out) == &kStreamClass && JS::GetReservedSlot(out, StreamCanvas).isObject())
    return true;
  return WebKit::Fail(cx, (std::string("'") + member +
      "' called on an object that does not implement interface CanvasCaptureMediaStream.").c_str());
}
bool CanvasGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args, "get canvas", &self)) return false;
  JS::RootedValue canvas(cx, JS::GetReservedSlot(self, StreamCanvas));
  if (!JS_WrapValue(cx, &canvas)) return false;
  args.rval().set(canvas);
  return true;
}
// Firefox queues a frame capture; this owner produces no frames (D135).
bool RequestFrame(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args, "requestFrame", &self)) return false;
  args.rval().setUndefined();
  return true;
}
}
bool InstallCanvasCapture(State& s) {
  JS::RootedObject parent(s.cx, s.stream.prototype);
  return s.kit.Define("CanvasCaptureMediaStream", &kStreamClass, parent, WebKit::Reject, &s.canvasCapture) &&
         JS_SetPrototype(s.cx, s.canvasCapture.constructor, s.stream.constructor) &&
         s.kit.Method(s.canvasCapture, "CanvasCaptureMediaStream", "requestFrame", RequestFrame, 0, 0) &&
         s.kit.Accessor(s.canvasCapture, "CanvasCaptureMediaStream", "canvas", CanvasGet, nullptr, 0) &&
         MediaConstructorLast(s, s.canvasCapture);
}
bool Realm::CaptureCanvas(JS::HandleObject canvas, JS::MutableHandleObject stream) {
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.kit.page);
  JS::RootedObject proto(s.cx, s.canvasCapture.prototype);
  JS::RootedObject object(s.cx, NewStream(s, proto));
  JS::RootedValue source(s.cx, JS::ObjectValue(*canvas));
  if (!object || !JS_WrapValue(s.cx, &source)) return false;
  JS::SetReservedSlot(object, StreamCanvas, source);
  // The capture's one video track: label "", unmuted, settings from the canvas.
  TrackSource init;
  init.video = true;
  init.origin = Origin::Canvas;
  init.canvas = &source.toObject();
  JS::RootedObject track(s.cx, NewTrack(s, init));
  if (!track || !AddStreamTrack(s.cx, object, track)) return false;
  stream.set(object);
  return true;
}
void Realm::SetCanvasSizePort(CanvasSizePort port, void* context) {
  state_->canvasSize = port;
  state_->canvasSizeContext = context;
}
}
