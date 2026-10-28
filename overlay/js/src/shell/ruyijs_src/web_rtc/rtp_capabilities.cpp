#include "web_rtc/state.h"
#include "domain/rtc/rtp_capabilities.h"
#include "web_rtc/profile.h"
#include "web_rtc/rtp_codecs.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"

namespace ruyijs::web_rtc {
namespace {
namespace rtc = ruyijs::domain::rtc;
bool PutString(JSContext* cx, JS::HandleObject object, const char* key, const char* value) {
  JS::RootedString string(cx, JS_NewStringCopyZ(cx, value));
  return string && JS_DefineProperty(cx, object, key, string, JSPROP_ENUMERATE);
}
bool AppendCodec(JSContext* cx, JS::HandleObject array, uint32_t index, const rtc::RtpCodec& spec) {
  JS::RootedObject codec(cx, JS_NewPlainObject(cx));
  if (!codec) return false;
  // WebIDL dictionary members are materialized in lexicographic order.
  if (spec.channels && !JS_DefineProperty(cx, codec, "channels", uint32_t(spec.channels), JSPROP_ENUMERATE)) return false;
  if (!JS_DefineProperty(cx, codec, "clockRate", spec.clock, JSPROP_ENUMERATE) ||
      !PutString(cx, codec, "mimeType", spec.mime)) return false;
  if (spec.fmtp && !PutString(cx, codec, "sdpFmtpLine", spec.fmtp)) return false;
  JS::RootedValue value(cx, JS::ObjectValue(*codec));
  return JS_DefineElement(cx, array, index, value, JSPROP_ENUMERATE);
}
bool AppendExtension(JSContext* cx, JS::HandleObject array, uint32_t index, const rtc::RtpExtension& spec) {
  JS::RootedObject object(cx, JS_NewPlainObject(cx));
  if (!object || !PutString(cx, object, "uri", spec.uri)) return false;
  JS::RootedValue value(cx, JS::ObjectValue(*object));
  return JS_DefineElement(cx, array, index, value, JSPROP_ENUMERATE);
}
}
// The codecs getCapabilities(kind) reports for one direction: the profile's
// "rtc" list when configured, otherwise the owner table. The entries point into
// the current profile and stay valid until it is reconfigured.
std::vector<ruyijs::domain::rtc::RtpCodec> CapabilityCodecs(
    bool audio, ruyijs::domain::rtc::RtpDirection direction) {
  namespace rtc = ruyijs::domain::rtc;
  std::vector<rtc::RtpCodec> out;
  const auto& configured = audio ? CurrentProfile().audioCodecs : CurrentProfile().videoCodecs;
  if (configured) {
    for (const auto& entry : *configured) {
      const rtc::RtpCodec codec{entry.mime.c_str(), entry.clock, entry.channels,
                                entry.fmtp.empty() ? nullptr : entry.fmtp.c_str(), entry.receiveOnly};
      if (codec.Supports(direction)) out.push_back(codec);
    }
  } else {
    for (const auto& codec : rtc::DefaultCodecs(audio ? rtc::RtpKind::Audio : rtc::RtpKind::Video))
      if (codec.Supports(direction)) out.push_back(codec);
  }
  return out;
}
bool RtpGetCapabilities(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const bool receiver = WebKit::Slot(args) == kReceiverGetCapabilities;
  if (!argc) return WebKit::Fail(cx, receiver ?
    "RTCRtpReceiver.getCapabilities: At least 1 argument required, but only 0 passed" :
    "RTCRtpSender.getCapabilities: At least 1 argument required, but only 0 passed");
  JS::RootedString text(cx, JS::ToString(cx, args[0]));
  bool audio = false, video = false;
  if (!text || !JS_StringEqualsAscii(cx, text, "audio", &audio) ||
      !JS_StringEqualsAscii(cx, text, "video", &video)) return false;
  if (!audio && !video) { args.rval().setNull(); return true; }
  const auto kind = audio ? rtc::RtpKind::Audio : rtc::RtpKind::Video;
  const auto direction = receiver ? rtc::RtpDirection::Receive : rtc::RtpDirection::Send;
  JS::RootedObject result(cx, JS_NewPlainObject(cx));
  JS::RootedObject codecs(cx, JS::NewArrayObject(cx, 0)), extensions(cx, JS::NewArrayObject(cx, 0));
  if (!result || !codecs || !extensions) return false;
  uint32_t index = 0;
  for (const auto& codec : CapabilityCodecs(audio, direction))
    if (!AppendCodec(cx, codecs, index++, codec)) return false;
  index = 0;
  for (const auto& extension : rtc::DefaultExtensions(kind))
    if (extension.Supports(direction) && !AppendExtension(cx, extensions, index++, extension)) return false;
  if (!JS_DefineProperty(cx, result, "codecs", codecs, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, result, "headerExtensions", extensions, JSPROP_ENUMERATE)) return false;
  args.rval().setObject(*result); return true;
}
}
