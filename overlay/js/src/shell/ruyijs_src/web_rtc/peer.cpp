#include "web_rtc/internal.h"

#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Realm.h"

namespace ruyijs::web_rtc {

// Builds a peer over an existing aggregate record. proto is the (possibly
// subclass) prototype resolved by the constructor.
JSObject* NewPeer(State& state, uint32_t id, JS::HandleObject proto) {
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.kit.page);
  PeerInfo info;
  if (!state.model.peer_info(id, &info)) return nullptr;
  JS::RootedObject peer(cx, state.kit.New(&kPeerClass, proto));
  JS::RootedObject senders(cx, JS::NewArrayObject(cx, 0));
  JS::RootedObject receivers(cx, JS::NewArrayObject(cx, 0));
  JS::RootedObject transceivers(cx, JS::NewArrayObject(cx, 0));
  JS::RootedObject channels(cx, JS::NewArrayObject(cx, 0));
  JS::RootedValue signaling(cx), iceConnection(cx), iceGathering(cx), connection(cx);
  if (!peer || !senders || !receivers || !transceivers || !channels ||
      !StringValue(cx, info.signaling_state, &signaling) ||
      !StringValue(cx, info.ice_connection_state, &iceConnection) ||
      !StringValue(cx, info.ice_gathering_state, &iceGathering) ||
      !StringValue(cx, info.connection_state, &connection))
    return nullptr;
  JS::SetReservedSlot(peer, kPeerId, JS::Int32Value(static_cast<int32_t>(id)));
  JS::SetReservedSlot(peer, kPeerLocalDescription, JS::NullValue());
  JS::SetReservedSlot(peer, kPeerRemoteDescription, JS::NullValue());
  JS::SetReservedSlot(peer, kPeerSignalingState, signaling);
  JS::SetReservedSlot(peer, kPeerIceConnectionState, iceConnection);
  JS::SetReservedSlot(peer, kPeerIceGatheringState, iceGathering);
  JS::SetReservedSlot(peer, kPeerConnectionState, connection);
  JS::SetReservedSlot(peer, kPeerSenders, JS::ObjectValue(*senders));
  JS::SetReservedSlot(peer, kPeerReceivers, JS::ObjectValue(*receivers));
  JS::SetReservedSlot(peer, kPeerTransceivers, JS::ObjectValue(*transceivers));
  JS::SetReservedSlot(peer, kPeerChannels, JS::ObjectValue(*channels));
  for (int32_t i = 0; i < kPeerHandlerCount; ++i)
    JS::SetReservedSlot(peer, kPeerHandlerBase + i, JS::NullValue());
  return state.events->Attach(peer) ? peer.get() : nullptr;
}

bool PeerCreate(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject proto(cx);
  if (!state->kit.ConstructProto(args, state->peer.prototype, &proto)) return false;
  const uint32_t id = state->model.create_peer();
  if (!id) return WebKit::Fail(cx, "RTCPeerConnection could not be created");
  JS::RootedObject peer(cx, NewPeer(*state, id, proto));
  if (!peer) return false;
  args.rval().setObject(*peer);
  return true;
}

bool PeerGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Brand(cx, args, &kPeerClass, &self))
    return false;
  args.rval().set(JS::GetReservedSlot(self, WebKit::Slot(args)));
  return true;
}

// Re-reads the aggregate into the peer's string slots, firing one change event
// per field that moved. The aggregate owns the values; the slots are the
// realm-local view the accessors read.
bool SyncPeer(State& state, JS::HandleObject peer) {
  JSContext* cx = state.cx;
  PeerInfo info;
  if (!state.model.peer_info(PeerId(peer), &info)) return true;
  struct Field {
    int32_t slot;
    const char* event;
    const std::string* value;
  };
  const Field fields[] = {
      {kPeerSignalingState, "signalingstatechange", &info.signaling_state},
      {kPeerIceConnectionState, "iceconnectionstatechange", &info.ice_connection_state},
      {kPeerIceGatheringState, "icegatheringstatechange", &info.ice_gathering_state},
      {kPeerConnectionState, "connectionstatechange", &info.connection_state},
  };
  for (const Field& field : fields) {
    JS::RootedValue previous(cx, JS::GetReservedSlot(peer, field.slot));
    std::string before;
    if (previous.isString()) {
      JS::RootedString text(cx, previous.toString());
      if (!ToUTF8(cx, text, &before)) return false;
    }
    if (before == *field.value) continue;
    JS::RootedValue next(cx);
    if (!StringValue(cx, *field.value, &next)) return false;
    JS::SetReservedSlot(peer, field.slot, next);
    if (!state.events->Fire(peer, field.event, false, false)) return false;
  }
  return true;
}

// Re-reads each channel of the peer, firing open/close for every transition.
bool SyncChannels(State& state, JS::HandleObject peer) {
  JSContext* cx = state.cx;
  JS::RootedObject channels(cx, &JS::GetReservedSlot(peer, kPeerChannels).toObject());
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, channels, &length)) return false;
  for (uint32_t i = 0; i < length; ++i) {
    JS::RootedValue element(cx);
    if (!JS_GetElement(cx, channels, i, &element) || !element.isObject()) continue;
    JS::RootedObject channel(cx, &element.toObject());
    ChannelInfo info;
    if (!state.model.channel_info(ChannelId(channel), &info)) continue;
    JS::RootedValue ready(cx, JS::GetReservedSlot(channel, kChannelReadyState));
    std::string before;
    if (ready.isString()) {
      JS::RootedString text(cx, ready.toString());
      if (!ToUTF8(cx, text, &before)) return false;
    }
    if (before != info.ready_state) {
      JS::RootedValue next(cx);
      if (!StringValue(cx, info.ready_state, &next)) return false;
      JS::SetReservedSlot(channel, kChannelReadyState, next);
      const bool opening = info.ready_state == "open";
      const bool closing = info.ready_state == "closed";
      if (opening && !state.events->Fire(channel, "open", false, false)) return false;
      if (closing && !state.events->Fire(channel, "close", false, false)) return false;
    }
    JS::SetReservedSlot(channel, kChannelBuffered,
                        JS::DoubleValue(static_cast<double>(info.buffered_amount)));
  }
  return true;
}

}  // namespace ruyijs::web_rtc
