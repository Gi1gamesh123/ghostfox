// WebCodecs AudioData in the web_media owner (Window and DedicatedWorker, as
// VideoFrame). Sample bytes are copied into the object's own Uint8Array; the
// layout, conversion and copy rules are domain/web_media/audio_samples.h, a port
// of Firefox dom/media/webcodecs/AudioData.cpp. Error texts measured on
// Firefox 158 (artifacts/mouser-datadome/audiodata_probe.js).
#include "web_media/audio_data_internal.h"
#include "web_media/image_decoder.h"
#include "web_media/media_runtime.h"
#include "web_media/state.h"
#include "domain/web_media/audio_samples.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/runtime.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/ArrayBuffer.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/SharedArrayBuffer.h"
#include "js/Wrapper.h"
#include "js/experimental/TypedData.h"
#include <cmath>
#include <cstring>
#include <string>

namespace ruyijs::web_media {
namespace dm = domain::web_media;
const JSClass kAudioDataClass = {"AudioData", JSCLASS_HAS_RESERVED_SLOTS(AdSlotCount)};
namespace audio_detail {
constexpr const char* kGetters[] = {"format", "sampleRate", "numberOfFrames", "numberOfChannels",
                                    "duration", "timestamp"};
bool Fail(JSContext* cx, const std::string& message) { return WebKit::Fail(cx, message.c_str()); }
bool Receiver(JSContext* cx, const JS::CallArgs& args, const std::string& member, JS::MutableHandleObject out) {
  out.set(args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (out && JS::GetClass(out) == &kAudioDataClass) return true;
  return Fail(cx, "'" + member + "' called on an object that does not implement interface AudioData.");
}
bool Closed(JSObject* self) { return JS::GetReservedSlot(self, AdBytes).isUndefined(); }
// One dictionary member; |present| is false for undefined.
bool Member(JSContext* cx, JS::HandleObject dict, const char* name, JS::MutableHandleValue value, bool* present) {
  if (!dict) { value.setUndefined(); *present = false; return true; }
  if (!JS_GetProperty(cx, dict, name, value)) return false;
  *present = !value.isUndefined();
  return true;
}
bool Required(JSContext* cx, const std::string& where, const char* dict, const char* name, bool present) {
  return present || Fail(cx, where + ": Missing required '" + name + "' member of " + dict + ".");
}
bool EnforceRange(JSContext* cx, JS::HandleValue value, double low, double high, const std::string& what, double* out) {
  double number = 0;
  if (!JS::ToNumber(cx, value, &number)) return false;
  if (!std::isfinite(number)) return Fail(cx, what + " is not a finite value.");
  number = std::trunc(number);
  if (number < low || number > high) return Fail(cx, what + " is outside the range [" +
      std::to_string(int64_t(low)) + ", " + std::to_string(uint64_t(high)) + "].");
  *out = number == 0 ? 0 : number;
  return true;
}
bool Format(JSContext* cx, JS::HandleValue value, const std::string& where, const char* dict, dm::SampleFormat* out) {
  JS::RootedString text(cx, JS::ToString(cx, value));
  std::string name;
  if (!text || !ToUTF8(cx, text, &name)) return false;
  if (dm::ParseSampleFormat(name, out)) return true;
  return Fail(cx, where + ": '" + name + "' (value of 'format' member of " + dict +
              ") is not a valid value for enumeration AudioSampleFormat.");
}
JSObject* Make(State& state, JS::HandleObject proto, double format, double rate, double frames,
               double channels, double timestamp, JS::HandleObject bytes) {
  JSContext* cx = state.cx;
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kAudioDataClass, proto));
  if (!object) return nullptr;
  JS::SetReservedSlot(object, AdFormat, JS::Int32Value(int32_t(format)));
  JS::SetReservedSlot(object, AdRate, JS::DoubleValue(rate));
  JS::SetReservedSlot(object, AdFrames, JS::DoubleValue(frames));
  JS::SetReservedSlot(object, AdChannels, JS::DoubleValue(channels));
  JS::SetReservedSlot(object, AdTimestamp, JS::DoubleValue(timestamp));
  JS::SetReservedSlot(object, AdBytes, JS::ObjectValue(*bytes));
  return object;
}
}  // namespace audio_detail
namespace {
using namespace audio_detail;
bool Constructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  const std::string where = "AudioData constructor";
  if (!args.isConstructing()) return trace.Finish(Fail(cx, where + ": 'new' is required"));
  if (argc < 1) return trace.Finish(Fail(cx, where + ": At least 1 argument required, but only 0 passed"));
  auto& state = *static_cast<State*>(WebKit::Owner(args));
  if (!args[0].isNullOrUndefined() && !args[0].isObject())
    return trace.Finish(Fail(cx, where + ": Argument 1 can't be converted to a dictionary."));
  JS::RootedObject init(cx, args[0].isObject() ? &args[0].toObject() : nullptr);
  // Dictionary members in WebIDL (lexicographic) order.
  JS::RootedValue data(cx), format(cx), channels(cx), frames(cx), rate(cx), timestamp(cx), transfer(cx);
  bool has = false;
  std::vector<uint8_t> bytes;
  const char* D = "AudioDataInit";
  if (!Member(cx, init, "data", &data, &has) || !Required(cx, where, D, "data", has) ||
      !ImageBuffer(cx, data, &bytes)) return trace.Finish(false);
  dm::SampleFormat sampleFormat;
  double numberOfChannels = 0, numberOfFrames = 0, sampleRate = 0, stamp = 0;
  if (!Member(cx, init, "format", &format, &has) || !Required(cx, where, D, "format", has) ||
      !Format(cx, format, where, D, &sampleFormat) ||
      !Member(cx, init, "numberOfChannels", &channels, &has) || !Required(cx, where, D, "numberOfChannels", has) ||
      !EnforceRange(cx, channels, 0, 4294967295.0, where + ": 'numberOfChannels' member of " + D, &numberOfChannels) ||
      !Member(cx, init, "numberOfFrames", &frames, &has) || !Required(cx, where, D, "numberOfFrames", has) ||
      !EnforceRange(cx, frames, 0, 4294967295.0, where + ": 'numberOfFrames' member of " + D, &numberOfFrames) ||
      !Member(cx, init, "sampleRate", &rate, &has) || !Required(cx, where, D, "sampleRate", has) ||
      !JS::ToNumber(cx, rate, &sampleRate)) return trace.Finish(false);
  if (!std::isfinite(sampleRate))
    return trace.Finish(Fail(cx, where + ": 'sampleRate' member of AudioDataInit is not a finite floating-point value."));
  sampleRate = double(float(sampleRate));
  if (!Member(cx, init, "timestamp", &timestamp, &has) || !Required(cx, where, D, "timestamp", has) ||
      !EnforceRange(cx, timestamp, -9007199254740991.0, 9007199254740991.0,
                    where + ": 'timestamp' member of " + D, &stamp) ||
      !Member(cx, init, "transfer", &transfer, &has)) return trace.Finish(false);
  // Firefox IsValidAudioDataInit.
  if (sampleRate < 1) return trace.Finish(Fail(cx, where + ": sampleRate must be positive"));
  if (numberOfFrames == 0) return trace.Finish(Fail(cx, where + ": mNumberOfFrames must be positive"));
  if (numberOfChannels == 0) return trace.Finish(Fail(cx, where + ": mNumberOfChannels must be positive"));
  const double needed = numberOfFrames * numberOfChannels * dm::BytesPerSample(sampleFormat);
  if (double(bytes.size()) < needed)
    return trace.Finish(Fail(cx, where + ": Array of size " + std::to_string(bytes.size()) +
                             " not big enough, should be at least " + std::to_string(uint64_t(needed))));
  bytes.resize(size_t(needed));
  if (has) {  // sequence<ArrayBuffer> transfer: detach after the copy.
    JS::RootedObject list(cx, transfer.isObject() ? &transfer.toObject() : nullptr);
    uint32_t length = 0;
    if (!list || !JS::GetArrayLength(cx, list, &length))
      return trace.Finish(list ? false : Fail(cx, where + ": 'transfer' member of AudioDataInit is not an iterable."));
    for (uint32_t i = 0; i < length; ++i) {
      JS::RootedValue item(cx);
      if (!JS_GetElement(cx, list, i, &item)) return trace.Finish(false);
      JS::RootedObject buffer(cx, item.isObject() ? js::CheckedUnwrapStatic(&item.toObject()) : nullptr);
      if (!buffer || !JS::IsArrayBufferObject(buffer))
        return trace.Finish(Fail(cx, where + ": Element of 'transfer' member of AudioDataInit is not an ArrayBuffer."));
      if (!JS::DetachArrayBuffer(cx, buffer)) return trace.Finish(false);
    }
  }
  JS::RootedObject store(cx, JS_NewUint8Array(cx, bytes.size()));
  if (!store) return trace.Finish(false);
  {
    JS::AutoCheckCannotGC nogc;
    bool shared = false;
    std::memcpy(JS_GetUint8ArrayData(store, &shared, nogc), bytes.data(), bytes.size());
  }
  JS::RootedObject proto(cx);
  if (!state.kit.ConstructProto(args, state.audioData.prototype, &proto)) return trace.Finish(false);
  JSObject* object = Make(state, proto, double(int(sampleFormat)), sampleRate, numberOfFrames,
                          numberOfChannels, stamp, store);
  if (!object) return trace.Finish(false);
  args.rval().setObject(*object);
  return trace.Finish(true);
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int slot = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args, std::string("get ") + kGetters[slot], &self)) return false;
  const bool closed = Closed(self);
  const double frames = JS::GetReservedSlot(self, AdFrames).toNumber();
  const double rate = JS::GetReservedSlot(self, AdRate).toNumber();
  switch (slot) {
    case 0: {
      if (closed) { args.rval().setNull(); return true; }
      const auto name = dm::kSampleFormatNames[JS::GetReservedSlot(self, AdFormat).toInt32()];
      JSString* text = JS_NewStringCopyN(cx, name.data(), name.size());
      if (!text) return false;
      args.rval().setString(text);
      return true;
    }
    case 1: args.rval().setNumber(closed ? 0 : rate); return true;
    case 2: args.rval().setNumber(closed ? 0 : frames); return true;
    case 3: args.rval().setNumber(closed ? 0 : JS::GetReservedSlot(self, AdChannels).toNumber()); return true;
    case 4: args.rval().setNumber(closed ? 0 : std::floor(frames * 1e6 / rate)); return true;
    default: args.rval().setNumber(JS::GetReservedSlot(self, AdTimestamp).toNumber()); return true;
  }
}
}

bool InstallAudioData(State& state) {
  using namespace audio_detail;
  JS::RootedObject parent(state.cx, JS::GetRealmObjectPrototype(state.cx));
  if (!parent || !state.kit.Define("AudioData", &kAudioDataClass, parent, Constructor, &state.audioData, 1)) return false;
  for (int i = 0; i < 4; ++i)
    if (!state.kit.Method(state.audioData, "AudioData", kMethods[i], Method, i == 0 ? 1 : i == 1 ? 2 : 0, i)) return false;
  for (int i = 0; i < 6; ++i)
    if (!state.kit.Accessor(state.audioData, "AudioData", kGetters[i], Get, nullptr, i)) return false;
  return MediaConstructorLast(state, state.audioData);
}
}
