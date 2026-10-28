#include "web_rtc/internal.h"
#include "js/Conversions.h"

namespace ruyijs::web_rtc {
namespace {

const char* const kClosed = "The RTCPeerConnection is closed";

// Creates the transceiver, appends it and its sender/receiver to the peer's
// stable JS arrays, and returns the sender in *sender.
bool AppendTransceiver(State& state, JS::HandleObject self, uint32_t id,
                       JS::MutableHandleObject sender,
                       JS::MutableHandleObject transceiverOut) {
  JSContext* cx = state.cx;
  JS::RootedObject transceiver(cx, NewTransceiver(state, self, id));
  if (!transceiver) return false;
  JS::RootedObject transceivers(cx,
                                &JS::GetReservedSlot(self, kPeerTransceivers).toObject());
  JS::RootedObject senders(cx, &JS::GetReservedSlot(self, kPeerSenders).toObject());
  JS::RootedObject receivers(cx, &JS::GetReservedSlot(self, kPeerReceivers).toObject());
  JS::RootedObject entrySender(
      cx, &JS::GetReservedSlot(transceiver, kTransceiverSender).toObject());
  JS::RootedObject entryReceiver(
      cx, &JS::GetReservedSlot(transceiver, kTransceiverReceiver).toObject());
  if (!Append(cx, transceivers, transceiver) || !Append(cx, senders, entrySender) ||
      !Append(cx, receivers, entryReceiver))
    return false;
  sender.set(entrySender);
  transceiverOut.set(transceiver);
  return true;
}

}  // namespace

bool AddTrackMethod(State& state, JS::HandleObject self, const JS::CallArgs& args,
                    JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  PeerInfo info;
  if (!state.model.peer_info(PeerId(self), &info) || info.closed)
    return ThrowError(state, "DOMException", "InvalidStateError", kClosed);
  if (!args.get(0).isObject())
    return ThrowError(state, "TypeError", nullptr, "addTrack requires a MediaStreamTrack");
  JS::RootedObject track(cx, &args[0].toObject());
  std::string kind;
  if (!ReadString(cx, track, "kind", &kind)) return false;
  const uint32_t id = state.model.add_transceiver(PeerId(self), kind, "sendrecv");
  if (!id) return ThrowError(state, "DOMException", "InvalidStateError", kClosed);
  JS::RootedObject sender(cx), transceiver(cx);
  if (!AppendTransceiver(state, self, id, &sender, &transceiver)) return false;
  JS::SetReservedSlot(sender, kSenderTrack, args[0]);
  result.setObject(*sender);
  return true;
}

bool AddTransceiverMethod(State& state, JS::HandleObject self, const JS::CallArgs& args,
                          JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  PeerInfo info;
  if (!state.model.peer_info(PeerId(self), &info) || info.closed)
    return ThrowError(state, "DOMException", "InvalidStateError", kClosed);
  std::string kind;
  std::string direction = "sendrecv";
  if (args.get(0).isString()) {
    JS::RootedString text(cx, args[0].toString());
    if (!ToUTF8(cx, text, &kind)) return false;
  } else if (args.get(0).isObject()) {
    JS::RootedObject init(cx, &args[0].toObject());
    if (!ReadString(cx, init, "kind", &kind)) return false;
  } else {
    return ThrowError(state, "TypeError", nullptr,
                      "addTransceiver requires a track or a kind");
  }
  // The optional init object is the second argument for both overloads.
  if (args.get(1).isObject()) {
    JS::RootedObject init(cx, &args[1].toObject());
    if (!ReadString(cx, init, "direction", &direction)) return false;
  }
  const uint32_t id = state.model.add_transceiver(PeerId(self), kind, direction);
  if (!id) return ThrowError(state, "DOMException", "InvalidStateError", kClosed);
  JS::RootedObject sender(cx), transceiver(cx);
  if (!AppendTransceiver(state, self, id, &sender, &transceiver)) return false;
  result.setObject(*transceiver);
  return true;
}

bool ApplyLegacyOfferOptions(State& state, JS::HandleObject self, JS::HandleValue options) {
  JSContext* cx = state.cx;
  if (!options.isObject()) return true;
  JS::RootedObject init(cx, &options.toObject());
  std::vector<uint32_t> transceivers;
  if (!state.model.peer_transceivers(PeerId(self), &transceivers)) return true;
  for (const char* kind : {"audio", "video"}) {
    JS::RootedValue wanted(cx);
    if (!JS_GetProperty(cx, init, kind[0] == 'a' ? "offerToReceiveAudio" : "offerToReceiveVideo", &wanted))
      return false;
    if (!JS::ToBoolean(wanted)) continue;
    bool present = false;
    for (uint32_t id : transceivers) {
      TransceiverInfo info;
      present = present || (state.model.transceiver_info(id, &info) && info.kind == kind);
    }
    if (present) continue;
    const uint32_t id = state.model.add_transceiver(PeerId(self), kind, "recvonly");
    JS::RootedObject sender(cx), transceiver(cx);
    if (!id || !AppendTransceiver(state, self, id, &sender, &transceiver)) return !id;
  }
  return true;
}

}  // namespace ruyijs::web_rtc
