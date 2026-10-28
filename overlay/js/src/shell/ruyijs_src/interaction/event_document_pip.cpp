// Public interface placeholder; no picture-in-picture Window or session factory.
#include "interaction/internal.h"
#include "runtime/native_constructor_trace.h"

namespace ruyijs::interaction {
namespace {
// No DocumentPictureInPictureEvent instance can exist, so every receiver fails
// the shared brand check (Firefox's text); a successful check cannot happen.
bool Window(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!Receiver(cx, args, Iface::DocumentPicture)) return false;
  return Error(cx, "TypeError", "DocumentPictureInPictureEvent has no window");
}
}
extern const JSPropertySpec kDocumentPictureProperties[] = {
  JS_PSG("window", Window, JSPROP_ENUMERATE), JS_PS_END};

bool DocumentPictureConstructor(JSContext* cx, unsigned n, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(n, vp);
  NativeConstructorTrace trace(cx, args);
  if (!args.isConstructing())
    return Error(cx, "TypeError", "DocumentPictureInPictureEvent requires new");
  // V8 resolves newTarget.prototype before invoking this public constructor's
  // argument checks. Keep that user-observable read without registering an Event.
  JS::RootedObject temporary(cx, JS_NewObjectForConstructor(cx, &kEventClass, args));
  if (!temporary) return false;
  if (State* state = RealmState(cx))
    state->Observe("DocumentPictureInPictureEvent", "constructor", int(ApiAccess::Construct));
  if (n < 2) return Error(cx, "TypeError", "DocumentPictureInPictureEvent requires 2 arguments");
  if (!args[0].isString()) return Error(cx, "TypeError", "Event type requires a string");
  if (!args[1].isObject()) return Error(cx, "TypeError", "Event options require an object");
  JS::RootedObject dict(cx, &args[1].toObject());
  JS::RootedValue ignored(cx);
  for (const char* name : {"bubbles", "cancelable", "composed", "window"})
    if (!PublicEventRead(cx, dict, name, &ignored)) return false;
  // Public ruyijs fails here even for a real Window. User supplements retain this
  // prototype and construct through Event, which remains the only event factory.
  return Error(cx, "TypeError", "DocumentPictureInPictureEvent native construction failed");
}
}  // namespace ruyijs::interaction
