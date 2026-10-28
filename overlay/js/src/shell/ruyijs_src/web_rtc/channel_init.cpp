// RTCPeerConnection.createDataChannel(label, RTCDataChannelInit) in Firefox
// 158's order (artifacts/rtc-channel/order): argument count, the label, the
// dictionary members in WebIDL (lexicographic) order with [EnforceRange]
// unsigned short conversion, then the closed check and the body checks of
// PeerConnection.sys.mjs. The aggregate creates the channel.
#include "web_rtc/internal.h"

#include <cmath>
#include <optional>
#include <string>

#include "js/Conversions.h"

namespace ruyijs::web_rtc {
namespace {
const char* const kWhere = "RTCPeerConnection.createDataChannel: ";

bool TypeError(State& state, const std::string& text) {
  return ThrowError(state, "TypeError", nullptr, text.c_str());
}
// One [EnforceRange] unsigned short member; *out stays empty when undefined.
bool Short(State& state, JS::HandleObject init, const char* name, std::optional<int32_t>* out) {
  JSContext* cx = state.cx;
  JS::RootedValue value(cx);
  if (!init || !JS_GetProperty(cx, init, name, &value)) return !init;
  if (value.isUndefined()) return true;
  double number = 0;
  if (!JS::ToNumber(cx, value, &number)) return false;
  const std::string member = std::string(kWhere) + "'" + name + "' member of RTCDataChannelInit is ";
  if (!std::isfinite(number))
    return TypeError(state, member + "not a finite value, so is out of range for unsigned short.");
  number = std::trunc(number);
  if (number < 0 || number > 65535) return TypeError(state, member + "out of range for unsigned short.");
  *out = int32_t(number);
  return true;
}
bool Boolean(State& state, JS::HandleObject init, const char* name, bool fallback, bool* out) {
  JS::RootedValue value(state.cx);
  if (init && !JS_GetProperty(state.cx, init, name, &value)) return false;
  *out = value.isUndefined() ? fallback : JS::ToBoolean(value);
  return true;
}
}  // namespace

bool CreateChannelMethod(State& state, JS::HandleObject self, const JS::CallArgs& args,
                         JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  if (!args.length()) return TypeError(state, std::string(kWhere) + "At least 1 argument required, but only 0 passed");
  JS::RootedString label(cx, JS::ToString(cx, args[0]));
  ChannelSpec spec;
  if (!label || !ToUTF8(cx, label, &spec.label)) return false;
  JS::HandleValue options = args.get(1);
  if (!options.isNullOrUndefined() && !options.isObject())
    return TypeError(state, std::string(kWhere) + "Argument 2 can't be converted to a dictionary.");
  JS::RootedObject init(cx, options.isObject() ? &options.toObject() : nullptr);
  std::optional<int32_t> id, lifetime, retransmits;
  JS::RootedValue protocol(cx);
  if (!Short(state, init, "id", &id) || !Short(state, init, "maxPacketLifeTime", &lifetime) ||
      !Short(state, init, "maxRetransmits", &retransmits) ||
      !Boolean(state, init, "negotiated", false, &spec.negotiated) ||
      !Boolean(state, init, "ordered", true, &spec.ordered) ||
      (init && !JS_GetProperty(cx, init, "protocol", &protocol)))
    return false;
  if (!protocol.isUndefined()) {
    JS::RootedString text(cx, JS::ToString(cx, protocol));
    if (!text || !ToUTF8(cx, text, &spec.protocol)) return false;
  }
  PeerInfo info;
  if (!state.model.peer_info(PeerId(self), &info) || info.closed)
    return ThrowError(state, "DOMException", "InvalidStateError", "Peer connection is closed");
  if (lifetime && retransmits) return TypeError(state, "Both maxPacketLifeTime and maxRetransmits cannot be provided");
  if (id && *id == 65535) return TypeError(state, "id cannot be 65535");
  if (spec.label.size() > 65535) return TypeError(state, "label cannot be longer than 65535 bytes");
  if (spec.protocol.size() > 65535) return TypeError(state, "protocol cannot be longer than 65535 bytes");
  if (spec.negotiated && !id) return TypeError(state, "id is required when negotiated is true");
  spec.id = id.value_or(-1);
  spec.max_packet_lifetime = lifetime.value_or(-1);
  spec.max_retransmits = retransmits.value_or(-1);
  const uint32_t channelId = state.model.create_channel(PeerId(self), spec);
  if (!channelId)
    return ThrowError(state, "DOMException", "InvalidStateError", "The RTCPeerConnection cannot create a channel");
  JS::RootedObject channel(cx, NewChannel(state, self, channelId));
  if (!channel) return false;
  JS::RootedObject channels(cx, &JS::GetReservedSlot(self, kPeerChannels).toObject());
  if (!Append(cx, channels, channel)) return false;
  result.setObject(*channel);
  return true;
}
}  // namespace ruyijs::web_rtc
