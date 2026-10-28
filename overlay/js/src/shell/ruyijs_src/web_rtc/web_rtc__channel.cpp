#include <string>
#include "web_rtc/internal.h"

#include "js/Conversions.h"
#include "js/Realm.h"

namespace ruyijs::web_rtc {
namespace {


// The byte size a send() would queue: UTF-8 length for a string, byteLength for
// an ArrayBuffer/view, size for a Blob. Anything else is a TypeError.
bool PayloadSize(JSContext* cx, JS::HandleValue data, size_t* out) {
  if (data.isString()) {
    std::string text;
    JS::RootedString string(cx, data.toString());
    if (!ToUTF8(cx, string, &text)) return false;
    *out = text.size();
    return true;
  }
  if (!data.isObject()) return false;
  JS::RootedObject object(cx, &data.toObject());
  static const char* const kSizes[] = {"byteLength", "size"};
  for (const char* name : kSizes) {
    JS::RootedValue value(cx);
    if (!JS_GetProperty(cx, object, name, &value)) return false;
    if (!value.isNumber()) continue;
    double number = 0;
    if (!JS::ToNumber(cx, value, &number)) return false;
    if (number < 0) return false;
    *out = static_cast<size_t>(number);
    return true;
  }
  return false;
}

bool SendData(State& state, JS::HandleObject self, const JS::CallArgs& args,
              JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  if (!args.length())
    return ThrowError(state, "TypeError", nullptr, "send requires data");
  size_t bytes = 0;
  if (!PayloadSize(cx, args.get(0), &bytes)) {
    if (JS_IsExceptionPending(cx)) return false;
    return ThrowError(state, "TypeError", nullptr,
                      "send requires a string, Blob or ArrayBuffer");
  }
  if (state.model.send(ChannelId(self), bytes) != SendResult::kSent)
    return ThrowError(state, "DOMException", "InvalidStateError",
                      "RTCDataChannel is not open");
  JS::RootedObject peer(cx, &JS::GetReservedSlot(self, kChannelPeer).toObject());
  if (!SyncChannels(state, peer)) return false;
  result.setUndefined();
  return true;
}

bool CloseDataChannel(State& state, JS::HandleObject self, JS::MutableHandleValue result) {
  if (!state.model.close_channel(ChannelId(self)))
    return ThrowError(state, "DOMException", "InvalidStateError",
                      "RTCDataChannel is unknown");
  JS::RootedObject peer(state.cx, &JS::GetReservedSlot(self, kChannelPeer).toObject());
  if (!SyncChannels(state, peer)) return false;
  result.setUndefined();
  return true;
}

}  // namespace

JSObject* NewChannel(State& state, JS::HandleObject peer, uint32_t id) {
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.kit.page);
  ChannelInfo info;
  if (!state.model.channel_info(id, &info)) return nullptr;
  JS::RootedObject channel(cx, state.kit.New(&kChannelClass, state.channel.prototype));
  JS::RootedValue label(cx), protocol(cx), ready(cx);
  JS::RootedValue maxRetransmits(cx, JS::NullValue());
  JS::RootedValue negotiatedId(cx, JS::NullValue());
  if (!channel || !StringValue(cx, info.label, &label) ||
      !StringValue(cx, info.protocol, &protocol) ||
      !StringValue(cx, info.ready_state, &ready))
    return nullptr;
  if (info.has_max_retransmits) maxRetransmits.setInt32(info.max_retransmits);
  if (info.has_id) negotiatedId.setInt32(info.negotiated_id);
  JS::RootedValue lifetime(cx, JS::NullValue());
  if (info.has_max_packet_lifetime) lifetime.setInt32(info.max_packet_lifetime);
  JS::RootedValue binaryType(cx);
  if (!StringValue(cx, "arraybuffer", &binaryType)) return nullptr;
  JS::SetReservedSlot(channel, kChannelId, JS::Int32Value(static_cast<int32_t>(id)));
  JS::SetReservedSlot(channel, kChannelLabel, label);
  JS::SetReservedSlot(channel, kChannelOrdered, JS::BooleanValue(info.ordered));
  JS::SetReservedSlot(channel, kChannelMaxRetransmits, maxRetransmits);
  JS::SetReservedSlot(channel, kChannelProtocol, protocol);
  JS::SetReservedSlot(channel, kChannelNegotiated, JS::BooleanValue(info.negotiated));
  JS::SetReservedSlot(channel, kChannelNegotiatedId, negotiatedId);
  JS::SetReservedSlot(channel, kChannelReadyState, ready);
  JS::SetReservedSlot(channel, kChannelBuffered,
                      JS::DoubleValue(static_cast<double>(info.buffered_amount)));
  JS::SetReservedSlot(channel, kChannelPeer, JS::ObjectValue(*peer));
  JS::SetReservedSlot(channel, kChannelMaxPacketLifeTime, lifetime);
  JS::SetReservedSlot(channel, kChannelLowThreshold, JS::DoubleValue(0));
  JS::SetReservedSlot(channel, kChannelBinaryType, binaryType);
  for (int32_t i = 0; i < kChannelHandlerCount; ++i)
    JS::SetReservedSlot(channel, kChannelHandlerBase + i, JS::NullValue());
  return state.events->Attach(channel) ? channel.get() : nullptr;
}

bool ChannelGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Brand(cx, args, &kChannelClass, &self)) return false;
  args.rval().set(JS::GetReservedSlot(self, WebKit::Slot(args)));
  return true;
}

// binaryType (an invalid enumeration value is ignored) and
// bufferedAmountLowThreshold (unsigned long: ToUint32).
bool ChannelSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Brand(cx, args, &kChannelClass, &self)) return false;
  const int32_t slot = WebKit::Slot(args);
  args.rval().setUndefined();
  if (slot == kChannelLowThreshold) {
    uint32_t threshold = 0;
    if (!JS::ToUint32(cx, args.get(0), &threshold)) return false;
    JS::SetReservedSlot(self, slot, JS::DoubleValue(threshold));
    return true;
  }
  JS::RootedString text(cx, JS::ToString(cx, args.get(0)));
  bool blob = false, buffer = false;
  if (!text || !JS_StringEqualsAscii(cx, text, "blob", &blob) ||
      !JS_StringEqualsAscii(cx, text, "arraybuffer", &buffer)) return false;
  if (blob || buffer) JS::SetReservedSlot(self, slot, JS::StringValue(text));
  return true;
}

bool ChannelMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx);
  if (!Brand(cx, args, &kChannelClass, &self)) return false;
  if (WebKit::Slot(args) == kChannelSend) return SendData(*state, self, args, args.rval());
  return CloseDataChannel(*state, self, args.rval());
}

// RTCDataChannel's prototype members, shared by the Window and Worker installers.
// RTCDataChannel's prototype members in Firefox 158 order, shared by the
// Window and Worker installers.
bool InstallChannelMembers(State& s) {
  struct Attribute { const char* name; int32_t slot; bool settable; };
  static const Attribute kAttributes[] = {
      {"label", kChannelLabel, false}, {"negotiated", kChannelNegotiated, false},
      {"ordered", kChannelOrdered, false}, {"maxPacketLifeTime", kChannelMaxPacketLifeTime, false},
      {"maxRetransmits", kChannelMaxRetransmits, false}, {"protocol", kChannelProtocol, false},
      {"id", kChannelNegotiatedId, false}, {"readyState", kChannelReadyState, false},
      {"bufferedAmount", kChannelBuffered, false}, {"bufferedAmountLowThreshold", kChannelLowThreshold, true}};
  // Handler order: onopen, onerror, onclosing, onclose, onmessage, onbufferedamountlow
  // (indices into kChannelHandlers).
  static const int32_t kHandlerOrder[] = {0, 3, 4, 2, 1, 5};
  const char* const name = "RTCDataChannel";
  if (!s.kit.Method(s.channel, name, "close", ChannelMethod, 0, kChannelClose) ||
      !s.kit.Method(s.channel, name, "send", ChannelMethod, 1, kChannelSend)) return false;
  for (const Attribute& a : kAttributes)
    if (!s.kit.Accessor(s.channel, name, a.name, ChannelGet, a.settable ? ChannelSet : nullptr, a.slot))
      return false;
  for (int32_t i : kHandlerOrder) {
    const std::string handler = std::string("on") + kChannelHandlers[i];
    if (!s.kit.Accessor(s.channel, name, handler.c_str(), HandlerGet, HandlerSet,
                        HandlerSlot(&kChannelClass, kChannelHandlerBase + i)))
      return false;
  }
  return s.kit.Accessor(s.channel, name, "binaryType", ChannelGet, ChannelSet, kChannelBinaryType);
}

}  // namespace ruyijs::web_rtc
