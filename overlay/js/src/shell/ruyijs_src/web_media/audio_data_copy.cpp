// AudioData allocationSize/copyTo/clone/close (Firefox AudioData.cpp
// ComputeCopyElementCount, AllocationSize, CopyTo, Clone, Close).
#include "web_media/audio_data_internal.h"
#include "web_media/media_runtime.h"
#include "web_media/state.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/ArrayBuffer.h"
#include "js/Object.h"
#include "js/SharedArrayBuffer.h"
#include "js/Wrapper.h"
#include "js/experimental/TypedData.h"
#include <string>

namespace ruyijs::web_media {
namespace dm = domain::web_media;
namespace audio_detail {
namespace {
// Firefox ComputeCopyElementCount over a parsed AudioDataCopyToOptions.
struct CopySpec { dm::SampleFormat format; uint32_t plane = 0, offset = 0, count = 0; uint64_t elements = 0; };
bool Spec(JSContext* cx, JS::HandleObject self, JS::HandleValue value, const std::string& where, CopySpec* spec) {
  if (!value.isNullOrUndefined() && !value.isObject())
    return Fail(cx, where + ": Argument can't be converted to a dictionary.");
  JS::RootedObject dict(cx, value.isObject() ? &value.toObject() : nullptr);
  JS::RootedValue format(cx), count(cx), offset(cx), plane(cx);
  bool hasFormat = false, hasCount = false, hasOffset = false, hasPlane = false;
  const char* D = "AudioDataCopyToOptions";
  spec->format = dm::SampleFormat(JS::GetReservedSlot(self, AdFormat).toInt32());
  double frameCount = 0, frameOffset = 0, planeIndex = 0;
  if (!Member(cx, dict, "format", &format, &hasFormat) ||
      (hasFormat && !Format(cx, format, where, D, &spec->format)) ||
      !Member(cx, dict, "frameCount", &count, &hasCount) ||
      (hasCount && !EnforceRange(cx, count, 0, 4294967295.0, where + ": 'frameCount' member of " + D, &frameCount)) ||
      !Member(cx, dict, "frameOffset", &offset, &hasOffset) ||
      (hasOffset && !EnforceRange(cx, offset, 0, 4294967295.0, where + ": 'frameOffset' member of " + D, &frameOffset)) ||
      !Member(cx, dict, "planeIndex", &plane, &hasPlane) || !Required(cx, where, D, "planeIndex", hasPlane) ||
      !EnforceRange(cx, plane, 0, 4294967295.0, where + ": 'planeIndex' member of " + D, &planeIndex)) return false;
  spec->plane = uint32_t(planeIndex);
  spec->offset = uint32_t(frameOffset);
  spec->count = hasCount ? uint32_t(frameCount) : UINT32_MAX;
  return true;
}
bool Elements(JSContext* cx, JS::HandleObject self, const std::string& where, CopySpec* spec) {
  const uint32_t channels = uint32_t(JS::GetReservedSlot(self, AdChannels).toNumber());
  const uint32_t frames = uint32_t(JS::GetReservedSlot(self, AdFrames).toNumber());
  if (dm::Interleaved(spec->format)) {
    if (spec->plane > 0) return WebKit::FailKind(cx, (where + ": Interleaved format, but plane index > 0").c_str(), JSEXN_RANGEERR);
  } else if (spec->plane >= channels) {
    return WebKit::FailKind(cx, (where + ": Plane index " + std::to_string(spec->plane) +
        " greater or equal than the number of channels " + std::to_string(channels)).c_str(), JSEXN_RANGEERR);
  }
  if (spec->offset >= frames)
    return WebKit::FailKind(cx, (where + ": Frame offset of " + std::to_string(spec->offset) +
        " greater or equal than frame count " + std::to_string(frames)).c_str(), JSEXN_RANGEERR);
  const uint32_t available = frames - spec->offset;
  if (spec->count == UINT32_MAX) spec->count = available;
  else if (spec->count > available)
    return WebKit::FailKind(cx, (where + ": Passed copy frame count of " + std::to_string(spec->count) +
        " greater than available source frames for copy of " + std::to_string(available)).c_str(), JSEXN_RANGEERR);
  spec->elements = uint64_t(spec->count) * (dm::Interleaved(spec->format) ? channels : 1);
  return true;
}
}  // namespace
bool Method(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int op = WebKit::Slot(args);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx);
  if (!Receiver(cx, args, kMethods[op], &self)) return false;
  const std::string where = std::string("AudioData.") + kMethods[op];
  const unsigned required = op == 0 ? 1 : op == 1 ? 2 : 0;
  if (argc < required)
    return Fail(cx, where + ": At least " + std::to_string(required) + " argument" + (required > 1 ? "s" : "") +
                " required, but only " + std::to_string(argc) + " passed");
  if (op == 3) {  // close(): detach the resource; idempotent.
    JS::SetReservedSlot(self, AdBytes, JS::UndefinedValue());
    args.rval().setUndefined();
    return true;
  }
  if (op == 2) {
    if (Closed(self)) return MediaException(state, "InvalidStateError", "AudioData.clone: No media resource in the AudioData now");
    JS::RootedObject bytes(cx, &JS::GetReservedSlot(self, AdBytes).toObject()), proto(cx);
    if (!JS_GetPrototype(cx, self, &proto)) return false;
    JSObject* copy = Make(state, proto, JS::GetReservedSlot(self, AdFormat).toInt32(),
        JS::GetReservedSlot(self, AdRate).toNumber(), JS::GetReservedSlot(self, AdFrames).toNumber(),
        JS::GetReservedSlot(self, AdChannels).toNumber(), JS::GetReservedSlot(self, AdTimestamp).toNumber(), bytes);
    if (!copy) return false;
    args.rval().setObject(*copy);
    return true;
  }
  if (Closed(self) && op == 0)
    return MediaException(state, "InvalidStateError", "AudioData.allocationSize: allocationSize called on detached AudioData");
  JS::RootedValue destination(cx, op == 1 ? args[0] : JS::UndefinedValue());
  CopySpec spec;
  if (!Spec(cx, self, args[op == 1 ? 1 : 0], where, &spec)) return false;
  if (Closed(self)) return MediaException(state, "InvalidStateError", "AudioData.copyTo: copyTo called on closed AudioData");
  if (!Elements(cx, self, where, &spec)) return false;
  const uint64_t size = spec.elements * dm::BytesPerSample(spec.format);
  if (op == 0) { args.rval().setNumber(double(size)); return true; }
  JS::RootedObject target(cx, destination.isObject() ? js::CheckedUnwrapStatic(&destination.toObject()) : nullptr);
  if (!target || (!JS_IsArrayBufferViewObject(target) && !JS::IsArrayBufferObject(target) && !JS::IsSharedArrayBufferObject(target)))
    return Fail(cx, where + ": Argument 1 could not be converted to any of: ArrayBufferView, ArrayBuffer.");
  const size_t length = JS_IsArrayBufferViewObject(target) ? JS_GetArrayBufferViewByteLength(target) :
      JS::IsArrayBufferObject(target) ? JS::GetArrayBufferByteLength(target) : JS::GetSharedArrayBufferByteLength(target);
  if (size > length)
    return WebKit::FailKind(cx, (where + ": destination buffer of length " + std::to_string(length) +
        " too small for copying " + std::to_string(size) + " elements").c_str(), JSEXN_RANGEERR);
  JS::RootedObject store(cx, &JS::GetReservedSlot(self, AdBytes).toObject());
  JS::AutoCheckCannotGC nogc;
  bool shared = false;
  uint8_t* to = JS_IsArrayBufferViewObject(target) ? static_cast<uint8_t*>(JS_GetArrayBufferViewData(target, &shared, nogc))
      : JS::IsArrayBufferObject(target) ? JS::GetArrayBufferData(target, &shared, nogc)
      : JS::GetSharedArrayBufferData(target, &shared, nogc);
  dm::CopyAudioSamples(JS_GetUint8ArrayData(store, &shared, nogc), to,
                       uint32_t(JS::GetReservedSlot(self, AdChannels).toNumber()),
                       uint32_t(JS::GetReservedSlot(self, AdFrames).toNumber()),
                       dm::SampleFormat(JS::GetReservedSlot(self, AdFormat).toInt32()), spec.format,
                       spec.count, spec.offset, spec.plane);
  args.rval().setUndefined();
  return true;
}
}  // namespace audio_detail
}
