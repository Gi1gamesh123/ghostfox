#ifndef RUYIJS_WEB_RTC_INTERNAL_H
#define RUYIJS_WEB_RTC_INTERNAL_H
#include <cstdint>
#include <string>
#include "js/CallArgs.h"
#include "js/CharacterEncoding.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "runtime/runtime.h"
#include "web_rtc/state.h"

namespace ruyijs::web_rtc {

// Dictionary readers shared by the peer, channel and description factories.
// A missing or null member leaves the output unchanged and succeeds.
inline bool ReadString(JSContext* cx, JS::HandleObject source, const char* name,
                       std::string* out) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, source, name, &value)) return false;
  if (value.isUndefined() || value.isNull()) return true;
  JS::RootedString text(cx, JS::ToString(cx, value));
  return text && ToUTF8(cx, text, out);
}

inline bool ReadBool(JSContext* cx, JS::HandleObject source, const char* name,
                     bool fallback) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, source, name, &value) || value.isUndefined()) return fallback;
  return JS::ToBoolean(value);
}

// Reads a nullable DOMString member; *present stays false for undefined/null.
inline bool ReadNullable(JSContext* cx, JS::HandleObject source, const char* name,
                         bool* present, std::string* out) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, source, name, &value)) return false;
  if (value.isUndefined() || value.isNull()) return true;
  JS::RootedString text(cx, JS::ToString(cx, value));
  if (!text) return false;
  *present = true;
  return ToUTF8(cx, text, out);
}

// *present stays false for a missing/null member; otherwise the value is
// truncated to int32.
inline bool ReadInt(JSContext* cx, JS::HandleObject source, const char* name,
                    bool* present, int32_t* out) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, source, name, &value)) return false;
  if (value.isUndefined() || value.isNull()) return true;
  double number = 0;
  if (!JS::ToNumber(cx, value, &number)) return false;
  *present = true;
  *out = static_cast<int32_t>(number);
  return true;
}

inline bool StringValue(JSContext* cx, const std::string& text,
                        JS::MutableHandleValue out) {
  JS::RootedString value(
      cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(text.data(), text.size())));
  if (!value) return false;
  out.setString(value);
  return true;
}

inline uint32_t PeerId(JS::HandleObject peer) {
  return static_cast<uint32_t>(JS::GetReservedSlot(peer, kPeerId).toInt32());
}

inline uint32_t ChannelId(JS::HandleObject channel) {
  return static_cast<uint32_t>(JS::GetReservedSlot(channel, kChannelId).toInt32());
}

// Peer methods split across translation units; PeerMethod dispatches to these.
bool CreateChannelMethod(State& state, JS::HandleObject self, const JS::CallArgs& args,
                         JS::MutableHandleValue result);
bool AddTrackMethod(State& state, JS::HandleObject self, const JS::CallArgs& args,
                    JS::MutableHandleValue result);
bool AddTransceiverMethod(State& state, JS::HandleObject self, const JS::CallArgs& args,
                          JS::MutableHandleValue result);
// createOffer's legacy RTCOfferOptions: a truthy offerToReceiveAudio/Video with
// no transceiver of that kind adds one recvonly transceiver (Firefox caps at one).
bool ApplyLegacyOfferOptions(State& state, JS::HandleObject self, JS::HandleValue options);

// Replaces the aggregate's placeholder offer with Firefox's SDP: session-only
// for an empty peer, else one media m-line per transceiver then the SCTP one. Returns false only when the OS random generator fails.
bool FillOffer(State& state, uint32_t peer, DescriptionSpec* spec);

}  // namespace ruyijs::web_rtc
#endif
