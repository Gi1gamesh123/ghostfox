#include "web_media/media_codecs.h"
#include "web_media/media_support.h"
#include "web_media/media_runtime.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/runtime.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include <cmath>
#include <cstdio>
namespace ruyijs::web_media {
const JSClass kMediaSourceClass = {"MediaSource", JSCLASS_HAS_RESERVED_SLOTS(SourceSlotCount)};
bool IsObjectUrlSource(JSObject* object) { return JS::GetClass(object) == &kMediaSourceClass; }
namespace {
JSObject* NewList(State& state) {
  JS::RootedObject object(state.cx, state.kit.New(&kSourceBufferListClass, state.sourceBufferList.prototype));
  if (!object || !state.events.Attach(object)) return nullptr;
  JS::SetReservedSlot(object, ListLength, JS::Int32Value(0));
  JS::SetReservedSlot(object, ListAdd, JS::NullValue());
  JS::SetReservedSlot(object, ListRemove, JS::NullValue());
  JS::SetReservedSlot(object, ListOwner, JS::PrivateValue(&state));
  return object.get();
}
bool Type(JSContext* cx, JS::HandleValue value, std::string* type) {
  JS::RootedString text(cx, JS::ToString(cx, value));
  return text && ToUTF8(cx, text, type);
}
}
bool MediaSourceConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  if (!args.isConstructing()) return WebKit::Fail(cx, "MediaSource constructor: 'new' is required");
  auto& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject object(cx, JS_NewObjectForConstructor(cx, &kMediaSourceClass, args));
  if (!object) return false;
  JS::RootedObject buffers(cx, NewList(state)), active(cx, NewList(state));
  if (!buffers || !active || !state.events.Attach(object)) return false;
  JS::SetReservedSlot(object, SourceBuffers, JS::ObjectValue(*buffers));
  JS::SetReservedSlot(object, SourceActive, JS::ObjectValue(*active));
  JS::SetReservedSlot(object, SourceDuration, JS::DoubleValue(NAN));
  JS::SetReservedSlot(object, SourceReady, JS::Int32Value(0));
  for (unsigned i = SourceOpen; i <= SourceClose; ++i) JS::SetReservedSlot(object, i, JS::NullValue());
  JS::SetReservedSlot(object, SourceOwner, JS::PrivateValue(&state));
  args.rval().setObject(*object); return trace.Finish(true);
}
bool MediaSourceStatic(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!argc) return WebKit::Fail(cx, "MediaSource.isTypeSupported: At least 1 argument required, but only 0 passed");
  std::string type;
  if (!Type(cx, args[0], &type)) return false;
  args.rval().setBoolean(MediaTypeSupported(type, false)); return true;
}
bool MediaSourceMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!MediaReceiver(cx, args.thisv(), &kMediaSourceClass, &self)) return false;
  auto& state = *static_cast<State*>(JS::GetReservedSlot(self, SourceOwner).toPrivate());
  const int op = WebKit::Slot(args);
  if (op == SourceReady) {
    JS::RootedString text(cx, JS_NewStringCopyZ(cx, "closed"));
    if (!text) return false;
    args.rval().setString(text); return true;
  }
  if (op < 7) return MediaGetSlot(cx, self, op, args.rval());
  if (op == 7) {
    if (!argc) return WebKit::Fail(cx, "MediaSource.addSourceBuffer: At least 1 argument required, but only 0 passed");
    std::string type;
    if (!Type(cx, args[0], &type)) return false;
    if (type.empty()) return WebKit::Fail(cx, "MediaSource.addSourceBuffer: Empty type");
    if (!MediaTypeSupported(type, false)) return MediaException(state, "NotSupportedError", "MediaSource.addSourceBuffer: Unknown type");
  } else if (op == 8) {
    JS::RootedObject buffer(cx);
    if (!MediaReceiver(cx, args.get(0), &kSourceBufferClass, &buffer)) {
      JS_ClearPendingException(cx);
      return WebKit::Fail(cx, "MediaSource.removeSourceBuffer: Argument 1 does not implement interface SourceBuffer.");
    }
  } else if (op == 9 && !args.get(0).isUndefined()) {
    std::string error;
    if (!Type(cx, args[0], &error)) return false;
    if (error != "network" && error != "decode") return WebKit::Fail(cx,
        ("MediaSource.endOfStream: '" + error + "' (value of argument 1) is not a valid value for enumeration MediaSourceEndOfStreamError.").c_str());
  } else if (op == 10) {
    if (argc < 2) return WebKit::Fail(cx, "MediaSource.setLiveSeekableRange requires two arguments");
    double start, end;
    if (!JS::ToNumber(cx, args[0], &start) || !JS::ToNumber(cx, args[1], &end)) return false;
    if (!std::isfinite(start) || !std::isfinite(end)) return WebKit::Fail(cx, "MediaSource.setLiveSeekableRange requires finite values");
  }
  return MediaInvalidState(state);
}
bool MediaSourceSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!MediaReceiver(cx, args.thisv(), &kMediaSourceClass, &self)) return false;
  if (!argc) return WebKit::Fail(cx, "MediaSource setter requires an argument");
  auto& state = *static_cast<State*>(JS::GetReservedSlot(self, SourceOwner).toPrivate());
  const int slot = WebKit::Slot(args);
  if (slot == SourceDuration) {
    double duration;
    if (!JS::ToNumber(cx, args[0], &duration)) return false;
    if (std::isnan(duration) || duration < 0) {
      char message[128];
      std::snprintf(message, sizeof(message), "MediaSource.duration setter: Invalid duration value %f", duration);
      return WebKit::Fail(cx, message);
    }
    return MediaInvalidState(state);
  }
  const char* names[] = {"sourceopen", "sourceended", "sourceclose"};
  if (!MediaHandlerSet(state, self, slot, names[slot - SourceOpen], args[0])) return false;
  args.rval().setUndefined(); return true;
}
}
