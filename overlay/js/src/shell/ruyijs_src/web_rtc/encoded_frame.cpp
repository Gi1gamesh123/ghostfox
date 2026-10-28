// RTCEncodedAudioFrame / RTCEncodedVideoFrame. Frames exist only inside an
// RTCRtpScriptTransformer's streams, which this owner does not run (D136), so
// no instance is ever created: the constructors validate their argument the
// way Firefox 158 does and then fail, because no original frame can exist.
// Shapes and texts were measured (artifacts/mouser-datadome/rtc_shape_probe.js).
#include <string>

#include "web_rtc/state.h"

namespace ruyijs::web_rtc {
namespace {
const JSClass kEncodedAudioClass = {"RTCEncodedAudioFrame", JSCLASS_HAS_RESERVED_SLOTS(1)};
const JSClass kEncodedVideoClass = {"RTCEncodedVideoFrame", JSCLASS_HAS_RESERVED_SLOTS(1)};
enum EncodedMember : int32_t { kGetMetadata = 0, kType, kTimestamp, kData, kSetData };
constexpr const char* kMemberNames[] = {"getMetadata", "get type", "get timestamp", "get data",
                                        "set data"};
constexpr int32_t kVideoFlag = 1 << 8;

bool Construct(JSContext* cx, unsigned argc, JS::Value* vp, const char* name) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const std::string prefix = std::string(name) + " constructor: ";
  if (!args.isConstructing()) return WebKit::Fail(cx, (prefix + "'new' is required").c_str());
  if (argc < 1)
    return WebKit::Fail(cx, (prefix + "At least 1 argument required, but only 0 passed").c_str());
  if (!args[0].isObject()) return WebKit::Fail(cx, (prefix + "Argument 1 is not an object.").c_str());
  return WebKit::Fail(cx, (prefix + "Argument 1 does not implement interface " + name + ".").c_str());
}
bool AudioCreate(JSContext* cx, unsigned argc, JS::Value* vp) {
  return Construct(cx, argc, vp, "RTCEncodedAudioFrame");
}
bool VideoCreate(JSContext* cx, unsigned argc, JS::Value* vp) {
  return Construct(cx, argc, vp, "RTCEncodedVideoFrame");
}

// Every member first brands its receiver; with no instances the brand check is
// the whole observable behavior.
bool Brand(JSContext* cx, const JS::CallArgs& args, int32_t slot) {
  const bool video = (slot & kVideoFlag) != 0;
  const std::string message = std::string("'") + kMemberNames[slot & 0xff] +
      "' called on an object that does not implement interface " +
      (video ? "RTCEncodedVideoFrame." : "RTCEncodedAudioFrame.");
  JS::RootedObject self(cx);
  return WebKit::Check(cx, args.thisv(), video ? &kEncodedVideoClass : &kEncodedAudioClass,
                       &self, message.c_str()) &&
         WebKit::Fail(cx, message.c_str());
}
bool Member(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  return Brand(cx, args, WebKit::Slot(args));
}
bool DataSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  return Brand(cx, args, (WebKit::Slot(args) & kVideoFlag) | kSetData);
}
}  // namespace

bool InstallEncodedFrames(State& s, JS::HandleObject objectProto) {
  auto& kit = s.kit;
  const char* const audio = "RTCEncodedAudioFrame";
  const char* const video = "RTCEncodedVideoFrame";
  return kit.Define(audio, &kEncodedAudioClass, objectProto, AudioCreate, &s.encodedAudio, 1) &&
         kit.Method(s.encodedAudio, audio, "getMetadata", Member, 0, kGetMetadata) &&
         kit.Accessor(s.encodedAudio, audio, "timestamp", Member, nullptr, kTimestamp) &&
         kit.Accessor(s.encodedAudio, audio, "data", Member, DataSet, kData) &&
         kit.Define(video, &kEncodedVideoClass, objectProto, VideoCreate, &s.encodedVideo, 1) &&
         kit.Method(s.encodedVideo, video, "getMetadata", Member, 0, kVideoFlag | kGetMetadata) &&
         kit.Accessor(s.encodedVideo, video, "type", Member, nullptr, kVideoFlag | kType) &&
         kit.Accessor(s.encodedVideo, video, "timestamp", Member, nullptr,
                      kVideoFlag | kTimestamp) &&
         kit.Accessor(s.encodedVideo, video, "data", Member, DataSet, kVideoFlag | kData);
}
}  // namespace ruyijs::web_rtc
