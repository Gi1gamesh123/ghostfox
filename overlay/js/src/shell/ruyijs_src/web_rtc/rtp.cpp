#include "web_rtc/internal.h"

#include <string>

#include "js/Array.h"
#include "js/Realm.h"
#include "jsapi.h"

namespace ruyijs::web_rtc {

// Senders and receivers exist only as transceiver halves. A sender's track
// starts null and addTrack stores the caller's track on it; a receiver's track
// is the remote track the Media owner creates for it.
JSObject* NewSender(State& state, uint32_t peer, const char* kind) {
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.kit.page);
  JS::RootedObject sender(cx, state.kit.New(&kSenderClass, state.sender.prototype));
  JS::RootedValue kindValue(cx);
  if (!sender || !StringValue(cx, kind ? kind : "", &kindValue)) return nullptr;
  JS::SetReservedSlot(sender, kSenderPeer, JS::Int32Value(static_cast<int32_t>(peer)));
  JS::SetReservedSlot(sender, kSenderKind, kindValue);
  JS::SetReservedSlot(sender, kSenderTrack, JS::NullValue());
  return sender.get();
}

JSObject* NewReceiver(State& state, uint32_t peer, const char* kind) {
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.kit.page);
  JS::RootedObject receiver(cx, state.kit.New(&kReceiverClass, state.receiver.prototype));
  JS::RootedValue kindValue(cx);
  if (!receiver || !StringValue(cx, kind ? kind : "", &kindValue)) return nullptr;
  JS::SetReservedSlot(receiver, kReceiverPeer, JS::Int32Value(static_cast<int32_t>(peer)));
  JS::SetReservedSlot(receiver, kReceiverKind, kindValue);
  // Firefox creates the receiver's remote track with the receiver; it stays
  // live (and muted) through transceiver.stop() and close().
  JS::RootedObject track(cx);
  if (!state.remoteTrack) {
    WebKit::Fail(cx, "RTCRtpReceiver requires the media track owner");
    return nullptr;
  }
  if (!state.remoteTrack(state.remoteTrackContext, kind && std::string(kind) == "video", &track) ||
      !JS_WrapObject(cx, &track))
    return nullptr;
  JS::SetReservedSlot(receiver, kReceiverTrack, JS::ObjectValue(*track));
  return receiver.get();
}

JSObject* NewTransceiver(State& state, JS::HandleObject owner, uint32_t id) {
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.kit.page);
  TransceiverInfo info;
  if (!state.model.transceiver_info(id, &info)) return nullptr;
  const uint32_t peer = PeerId(owner);
  JS::RootedObject transceiver(cx,
                               state.kit.New(&kTransceiverClass, state.transceiver.prototype));
  JS::RootedObject sender(cx, NewSender(state, peer, info.kind.c_str()));
  JS::RootedObject receiver(cx, NewReceiver(state, peer, info.kind.c_str()));
  JS::RootedValue mid(cx, JS::NullValue()), direction(cx), kind(cx);
  if (!transceiver || !sender || !receiver ||
      !StringValue(cx, info.direction, &direction) || !StringValue(cx, info.kind, &kind))
    return nullptr;
  if (!info.mid.empty() && !StringValue(cx, info.mid, &mid)) return nullptr;
  JS::SetReservedSlot(transceiver, kTransceiverId, JS::Int32Value(static_cast<int32_t>(id)));
  JS::SetReservedSlot(transceiver, kTransceiverPeer, JS::Int32Value(static_cast<int32_t>(peer)));
  JS::SetReservedSlot(transceiver, kTransceiverMid, mid);
  JS::SetReservedSlot(transceiver, kTransceiverDirection, direction);
  JS::SetReservedSlot(transceiver, kTransceiverKind, kind);
  JS::SetReservedSlot(transceiver, kTransceiverSender, JS::ObjectValue(*sender));
  JS::SetReservedSlot(transceiver, kTransceiverReceiver, JS::ObjectValue(*receiver));
  return transceiver.get();
}

bool SenderGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Brand(cx, args, &kSenderClass, &self))
    return false;
  args.rval().set(JS::GetReservedSlot(self, WebKit::Slot(args)));
  return true;
}

bool ReceiverGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Brand(cx, args, &kReceiverClass, &self))
    return false;
  args.rval().set(JS::GetReservedSlot(self, WebKit::Slot(args)));
  return true;
}

bool TransceiverGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Brand(cx, args, &kTransceiverClass, &self))
    return false;
  args.rval().set(JS::GetReservedSlot(self, WebKit::Slot(args)));
  return true;
}

}  // namespace ruyijs::web_rtc
