// RTCRtpTransceiver members in Firefox 158 prototype order: stop,
// setCodecPreferences, mid, sender, receiver, stopped, direction,
// currentDirection. Direction and stopping live in the session aggregate
// (stop_transceiver / set_transceiver_direction); texts were measured in
// Firefox (artifacts/mouser-datadome/rtc_shape_probe.js, rtc2_probe.js).
#include <cctype>
#include <string>

#include "js/Conversions.h"
#include "js/ForOfIterator.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "runtime/runtime.h"
#include "web_rtc/rtp_codecs.h"
#include "web_rtc/state.h"

namespace ruyijs::web_rtc {
namespace {
namespace rtc = ruyijs::domain::rtc;
constexpr const char* kDirections[] = {"sendrecv", "sendonly", "recvonly", "inactive"};
enum TransceiverModelResult { kModelApplied = 0, kModelClosed = 2, kModelStopping = 3 };

bool Receiver(JSContext* cx, const JS::CallArgs& args, const char* member,
              JS::MutableHandleObject self) {
  const std::string message = std::string("'") + member +
      "' called on an object that does not implement interface RTCRtpTransceiver.";
  return WebKit::Check(cx, args.thisv(), &kTransceiverClass, self, message.c_str());
}
uint32_t Id(JS::HandleObject self) {
  return static_cast<uint32_t>(JS::GetReservedSlot(self, kTransceiverId).toInt32());
}
std::string Lowered(std::string text) {
  for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return text;
}

bool Stop(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx);
  if (!Receiver(cx, args, "stop", &self)) return false;
  args.rval().setUndefined();
  if (state.model.stop_transceiver(Id(self)) == kModelClosed)
    return ThrowError(state, "DOMException", "InvalidStateError",
                      "RTCRtpTransceiver.stop: Peer connection is closed");
  return true;
}

bool DirectionGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx);
  if (!Receiver(cx, args, "get direction", &self)) return false;
  TransceiverInfo info;
  if (!state.model.transceiver_info(Id(self), &info)) {
    args.rval().set(JS::GetReservedSlot(self, kTransceiverDirection));
    return true;
  }
  JSString* text = JS_NewStringCopyN(cx, info.direction.data(), info.direction.size());
  if (!text) return false;
  args.rval().setString(text);
  return true;
}

bool DirectionSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx);
  if (!Receiver(cx, args, "set direction", &self)) return false;
  JS::RootedString text(cx, JS::ToString(cx, args.get(0)));
  std::string direction;
  if (!text || !ToUTF8(cx, text, &direction)) return false;
  args.rval().setUndefined();
  if (direction == "stopped")
    return ThrowError(state, "TypeError", nullptr,
                      "RTCRtpTransceiver.direction setter: Cannot use \"stopped\" in setDirection!");
  bool known = false;
  for (const char* name : kDirections) known = known || direction == name;
  if (!known) return true;  // An invalid enumeration value is ignored.
  const int result = state.model.set_transceiver_direction(Id(self), direction);
  if (result == kModelClosed || result == kModelStopping)
    return ThrowError(state, "DOMException", "InvalidStateError",
                      "RTCRtpTransceiver.direction setter: Transceiver is stopping/stopped!");
  return true;
}

// [[Stopped]] and [[CurrentDirection]] change only through negotiation, which
// this owner does not run: stopped stays false, currentDirection stays null.
bool NegotiatedGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const bool current = WebKit::Slot(args) != 0;
  JS::RootedObject self(cx);
  if (!Receiver(cx, args, current ? "get currentDirection" : "get stopped", &self)) return false;
  if (current) args.rval().setNull(); else args.rval().setBoolean(false);
  return true;
}

// One RTCRtpCodec dictionary, members read in WebIDL (lexicographic) order.
bool CodecMatches(State& state, JS::HandleValue item, const std::vector<rtc::RtpCodec>& known,
                  const std::string& where, bool* matched, std::string* mime) {
  JSContext* cx = state.cx;
  if (!item.isNullOrUndefined() && !item.isObject())
    return ThrowError(state, "TypeError", nullptr,
                      (where + "Element of argument 1 can't be converted to a dictionary.").c_str());
  JS::RootedObject codec(cx, item.isObject() ? &item.toObject() : nullptr);
  JS::RootedValue member(cx);
  auto get = [&](const char* name) {
    if (codec) return JS_GetProperty(cx, codec, name, &member);
    member.setUndefined();
    return true;
  };
  double channels = -1, clock = 0;
  std::string fmtp;
  bool hasFmtp = false;
  if (!get("channels") || (!member.isUndefined() && !JS::ToNumber(cx, member, &channels)) ||
      !get("clockRate"))
    return false;
  if (member.isUndefined())
    return ThrowError(state, "TypeError", nullptr,
                      (where + "Missing required 'clockRate' member of RTCRtpCodec.").c_str());
  if (!JS::ToNumber(cx, member, &clock) || !get("mimeType")) return false;
  if (member.isUndefined())
    return ThrowError(state, "TypeError", nullptr,
                      (where + "Missing required 'mimeType' member of RTCRtpCodec.").c_str());
  JS::RootedString text(cx, JS::ToString(cx, member));
  if (!text || !ToUTF8(cx, text, mime) || !get("sdpFmtpLine")) return false;
  if (!member.isUndefined()) {
    JS::RootedString line(cx, JS::ToString(cx, member));
    if (!line || !ToUTF8(cx, line, &fmtp)) return false;
    hasFmtp = true;
  }
  *matched = false;
  for (const auto& entry : known)
    *matched = *matched ||
        (Lowered(entry.mime) == Lowered(*mime) && entry.clock == static_cast<uint32_t>(clock) &&
         (channels < 0 || entry.channels == static_cast<uint16_t>(channels)) &&
         (!hasFmtp || fmtp == (entry.fmtp ? entry.fmtp : "")));
  return true;
}

bool SetCodecPreferences(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  const std::string where = "RTCRtpTransceiver.setCodecPreferences: ";
  JS::RootedObject self(cx);
  if (!Receiver(cx, args, "setCodecPreferences", &self)) return false;
  if (argc < 1)
    return ThrowError(state, "TypeError", nullptr,
                      (where + "At least 1 argument required, but only 0 passed").c_str());
  JS::ForOfIterator iterator(cx);
  if (!iterator.init(args[0], JS::ForOfIterator::AllowNonIterable)) return false;
  if (!iterator.valueIsIterable())
    return ThrowError(state, "TypeError", nullptr,
                      (where + "Argument 1 can't be converted to a sequence.").c_str());
  TransceiverInfo info;
  state.model.transceiver_info(Id(self), &info);
  const auto known = CapabilityCodecs(info.kind == "audio", rtc::RtpDirection::Receive);
  JS::RootedValue item(cx);
  while (true) {
    bool done = false;
    if (!iterator.next(&item, &done)) return false;
    if (done) break;
    bool matched = false;
    std::string mime;
    if (!CodecMatches(state, item, known, where, &matched, &mime)) return false;
    if (!matched)
      return ThrowError(state, "DOMException", "InvalidModificationError",
                        (where + "Codec " + mime + " not in capabilities").c_str());
  }
  // The preference only orders codecs in a negotiation this owner does not run.
  args.rval().setUndefined();
  return true;
}
}  // namespace

bool InstallTransceiverMembers(State& s) {
  auto& kit = s.kit;
  const char* const name = "RTCRtpTransceiver";
  return kit.Method(s.transceiver, name, "stop", Stop, 0, 0) &&
         kit.Method(s.transceiver, name, "setCodecPreferences", SetCodecPreferences, 1, 0) &&
         kit.Accessor(s.transceiver, name, "mid", TransceiverGet, nullptr, kTransceiverMid) &&
         kit.Accessor(s.transceiver, name, "sender", TransceiverGet, nullptr, kTransceiverSender) &&
         kit.Accessor(s.transceiver, name, "receiver", TransceiverGet, nullptr,
                      kTransceiverReceiver) &&
         kit.Accessor(s.transceiver, name, "stopped", NegotiatedGet, nullptr, 0) &&
         kit.Accessor(s.transceiver, name, "direction", DirectionGet, DirectionSet, 0) &&
         kit.Accessor(s.transceiver, name, "currentDirection", NegotiatedGet, nullptr, 1);
}
}  // namespace ruyijs::web_rtc
