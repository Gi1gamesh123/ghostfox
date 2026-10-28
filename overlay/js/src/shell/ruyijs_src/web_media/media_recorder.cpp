#include "web_media/media_codecs.h"
#include "web_media/media_runtime.h"
#include "web_media/media_support.h"
#include "web_media/stream.h"
#include "runtime/native_constructor_trace.h"
#include "jsapi.h"
#include "js/Object.h"
#include "runtime/runtime.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
namespace ruyijs::web_media {
const JSClass kMediaRecorderClass = {"MediaRecorder", JSCLASS_HAS_RESERVED_SLOTS(RecorderSlotCount)};
bool MediaRecorderConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  if (!args.isConstructing()) return WebKit::Fail(cx, "MediaRecorder constructor: 'new' is required");
  if (!argc) return WebKit::Fail(cx, "MediaRecorder constructor: 0 is not a valid argument count for any overload.");
  auto& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject stream(cx);
  if (!StreamArgument(cx, args[0], &stream)) return false;
  std::string mime;
  uint32_t audio = 128000, video = 2500000;
  if (!args.get(1).isNullOrUndefined()) {
    if (!args[1].isObject()) return WebKit::Fail(cx, "MediaRecorder options are not an object");
    JS::RootedObject options(cx, &args[1].toObject());
    JS::RootedValue value(cx);
    for (const char* key : {"audioBitsPerSecond", "bitsPerSecond", "mimeType", "videoBitsPerSecond"}) {
      if (!JS_GetProperty(cx, options, key, &value)) return false;
      if (value.isUndefined()) continue;
      if (std::string_view(key) == "mimeType") {
        JS::RootedString text(cx, JS::ToString(cx, value));
        if (!text || !ToUTF8(cx, text, &mime)) return false;
      } else {
        uint32_t number;
        if (!JS::ToUint32(cx, value, &number)) return false;
        if (key[0] == 'a') audio = number;
        if (key[0] == 'v') video = number;
      }
    }
  }
  if (!MediaTypeSupported(mime, true)) return MediaException(state, "NotSupportedError",
      ("MediaRecorder constructor: " + mime + " indicates an unsupported container").c_str());
  JS::RootedObject object(cx, JS_NewObjectForConstructor(cx, &kMediaRecorderClass, args));
  JS::RootedString type(cx, JS_NewStringCopyZ(cx, mime.c_str())), inactive(cx, JS_NewStringCopyZ(cx, "inactive"));
  if (!object || !type || !inactive || !state.events.Attach(object)) return false;
  JS::SetReservedSlot(object, RecorderStream, args[0]);
  JS::SetReservedSlot(object, RecorderMime, JS::StringValue(type));
  JS::SetReservedSlot(object, RecorderState, JS::StringValue(inactive));
  JS::SetReservedSlot(object, RecorderVideoRate, JS::DoubleValue(video));
  JS::SetReservedSlot(object, RecorderAudioRate, JS::DoubleValue(audio));
  for (unsigned i = RecorderStart; i < RecorderOwner; ++i) JS::SetReservedSlot(object, i, JS::NullValue());
  JS::SetReservedSlot(object, RecorderOwner, JS::PrivateValue(&state));
  args.rval().setObject(*object); return trace.Finish(true);
}
bool MediaRecorderStatic(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!argc) return WebKit::Fail(cx, "MediaRecorder.isTypeSupported: At least 1 argument required, but only 0 passed");
  JS::RootedString text(cx, JS::ToString(cx, args[0]));
  std::string type;
  if (!text || !ToUTF8(cx, text, &type)) return false;
  args.rval().setBoolean(MediaTypeSupported(type, true)); return true;
}
bool MediaRecorderMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!MediaReceiver(cx, args.thisv(), &kMediaRecorderClass, &self)) return false;
  auto& state = *static_cast<State*>(JS::GetReservedSlot(self, RecorderOwner).toPrivate());
  const int op = WebKit::Slot(args);
  if (op < 100) return MediaGetSlot(cx, self, op, args.rval());
  const char* names[] = {"start", "stop", "pause", "resume", "requestData"};
  if (op == 101) { args.rval().setUndefined(); return true; }
  if (op == 100) {
    uint32_t timeslice;
    if (!args.get(0).isUndefined() && !JS::ToUint32(cx, args[0], &timeslice)) return false;
    return MediaException(state, "NotSupportedError", "MediaRecorder.start: The MediaStream is inactive");
  }
  return MediaException(state, "InvalidStateError", (std::string("MediaRecorder.") + names[op - 100] + ": The MediaRecorder is inactive").c_str());
}
bool MediaRecorderSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!MediaReceiver(cx, args.thisv(), &kMediaRecorderClass, &self)) return false;
  if (!argc) return WebKit::Fail(cx, "MediaRecorder setter requires an argument");
  auto& state = *static_cast<State*>(JS::GetReservedSlot(self, RecorderOwner).toPrivate());
  const unsigned slot = WebKit::Slot(args);
  const char* names[] = {"start", "stop", "dataavailable", "pause", "resume", "error"};
  if (!MediaHandlerSet(state, self, slot, names[slot - RecorderStart], args[0])) return false;
  args.rval().setUndefined(); return true;
}
}
