#include "web_rtc/internal.h"

#include "js/CallAndConstruct.h"
#include "js/ValueArray.h"
#include "web_rtc/profile.h"

namespace ruyijs::web_rtc {
namespace {

const char* const kIncompatible = "RTCPeerConnection member called on an incompatible receiver";
const char* const kClosed = "The RTCPeerConnection is closed";
const char* const kBadState = "The description is illegal in the current signaling state";

bool RejectClosed(State& state, JS::MutableHandleValue result) {
  return RejectError(state, "DOMException", "InvalidStateError", kClosed, result);
}

bool CreateOfferAnswer(State& state, JS::HandleObject self, bool offer, JS::HandleValue options,
                       JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  PeerInfo info;
  if (!state.model.peer_info(PeerId(self), &info) || info.closed)
    return RejectClosed(state, result);
  const uint32_t id = PeerId(self);
  if (offer && !ApplyLegacyOfferOptions(state, self, options)) return false;
  DescriptionSpec spec = offer ? state.model.offer(id) : state.model.answer(id);
  if (offer && !FillOffer(state, id, &spec)) return false;
  JS::RootedObject description(cx, NewDescription(state, spec, state.description.prototype));
  if (!description) return false;
  JS::RootedValue value(cx, JS::ObjectValue(*description));
  return state.kit.Resolve(value, result);
}

bool SetDescription(State& state, JS::HandleObject self, const JS::CallArgs& args,
                    bool local, JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  PeerInfo info;
  if (!state.model.peer_info(PeerId(self), &info) || info.closed)
    return RejectClosed(state, result);
  if (!args.get(0).isObject())
    return RejectError(state, "TypeError", nullptr,
                       "setLocalDescription requires a description", result);
  JS::RootedObject dictionary(cx, &args[0].toObject());
  DescriptionSpec spec;
  if (!ReadString(cx, dictionary, "type", &spec.type) ||
      !ReadString(cx, dictionary, "sdp", &spec.sdp))
    return false;
  const uint32_t id = PeerId(self);
  const bool gather = local && info.ice_gathering_state == "new";
  const SetResult status =
      local ? state.model.set_local(id, spec) : state.model.set_remote(id, spec);
  if (status == SetResult::kInvalidState)
    return RejectError(state, "DOMException", "InvalidStateError", kBadState, result);
  if (status != SetResult::kApplied) return RejectClosed(state, result);
  JS::RootedObject stored(cx);
  if (JS::GetClass(dictionary) == &kDescriptionClass) stored.set(dictionary);
  else stored.set(NewDescription(state, spec, state.description.prototype));
  if (!stored) return false;
  JS::SetReservedSlot(self, local ? kPeerLocalDescription : kPeerRemoteDescription,
                      JS::ObjectValue(*stored));
  if (!SyncPeer(state, self) || !SyncChannels(state, self)) return false;
  // ICE gathering trickles its candidates after this promise settles (ice_gathering.cpp).
  if (gather && !StartGathering(state, self, spec.sdp)) return false;
  return state.kit.Resolve(JS::UndefinedHandleValue, result);
}

bool AddIceCandidate(State& state, JS::HandleObject self, const JS::CallArgs& args,
                     JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  if (!args.get(0).isObject())
    return RejectError(state, "TypeError", nullptr,
                       "addIceCandidate requires an RTCIceCandidateInit", result);
  JS::RootedObject dictionary(cx, &args[0].toObject());
  JS::RootedValue mid(cx), index(cx);
  if (!JS_GetProperty(cx, dictionary, "sdpMid", &mid) ||
      !JS_GetProperty(cx, dictionary, "sdpMLineIndex", &index))
    return false;
  const bool midMissing = mid.isUndefined() || mid.isNull();
  const bool indexMissing = index.isUndefined() || index.isNull();
  if (midMissing && indexMissing)
    return RejectError(state, "TypeError", nullptr,
                       "addIceCandidate requires sdpMid or sdpMLineIndex", result);
  switch (state.model.add_ice_candidate(PeerId(self))) {
    case CandidateResult::kApplied:
      return state.kit.Resolve(JS::UndefinedHandleValue, result);
    case CandidateResult::kClosed:
      return RejectClosed(state, result);
    default:
      return RejectError(state, "DOMException", "InvalidStateError",
                         "addIceCandidate requires a remote description", result);
  }
}

bool ReturnSlot(State& state, JS::HandleObject self, int32_t slot,
                JS::MutableHandleValue result) {
  PeerInfo info;
  if (!state.model.peer_info(PeerId(self), &info) || info.closed)
    return ThrowError(state, "DOMException", "InvalidStateError", kClosed);
  result.set(JS::GetReservedSlot(self, slot));
  return true;
}

bool ClosePeer(State& state, JS::HandleObject self, JS::MutableHandleValue result) {
  if (!state.model.close_peer(PeerId(self))) return WebKit::Fail(state.cx, kIncompatible);
  JS::SetReservedSlot(self, kPeerLocalDescription, JS::NullValue());
  JS::SetReservedSlot(self, kPeerRemoteDescription, JS::NullValue());
  if (!SyncPeer(state, self) || !SyncChannels(state, self)) return false;
  result.setUndefined();
  return true;
}

}  // namespace

bool PeerMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx);
  const int32_t op = WebKit::Slot(args);
  if (!Brand(cx, args, &kPeerClass, &self)) {
    // A promise-returning operation reports a foreign receiver as a rejection.
    const bool promise = op == kPeerCreateOffer || op == kPeerCreateAnswer ||
                         op == kPeerSetLocal || op == kPeerSetRemote || op == kPeerAddIceCandidate;
    JS::RootedValue error(cx);
    if (!promise || !JS_GetPendingException(cx, &error)) return false;
    JS_ClearPendingException(cx);
    return state->kit.RejectWith(error, args.rval());
  }
  switch (op) {
    case kPeerCreateOffer: return CreateOfferAnswer(*state, self, true, args.get(0), args.rval());
    case kPeerCreateAnswer: return CreateOfferAnswer(*state, self, false, args.get(0), args.rval());
    case kPeerSetLocal: return SetDescription(*state, self, args, true, args.rval());
    case kPeerSetRemote: return SetDescription(*state, self, args, false, args.rval());
    case kPeerAddIceCandidate: return AddIceCandidate(*state, self, args, args.rval());
    case kPeerCreateDataChannel:
      return CreateChannelMethod(*state, self, args, args.rval());
    case kPeerAddTrack: return AddTrackMethod(*state, self, args, args.rval());
    case kPeerAddTransceiver:
      return AddTransceiverMethod(*state, self, args, args.rval());
    case kPeerGetSenders: return ReturnSlot(*state, self, kPeerSenders, args.rval());
    case kPeerGetReceivers: return ReturnSlot(*state, self, kPeerReceivers, args.rval());
    case kPeerGetTransceivers:
      return ReturnSlot(*state, self, kPeerTransceivers, args.rval());
    case kPeerClose: return ClosePeer(*state, self, args.rval());
    default: return WebKit::Fail(cx, "unknown RTCPeerConnection method");
  }
}

}  // namespace ruyijs::web_rtc
