#include "web_rtc/state.h"
#include "runtime/page_helpers.h"

#include <string>

#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::web_rtc {

// RTCPeerConnection and RTCDataChannel are EventTargets, so their prototypes
// extend the interaction realm's one EventTarget prototype and every event
// travels the shared dispatch tree. RTCRtpSender/Receiver/Transceiver,
// RTCSessionDescription and RTCIceCandidate are plain interfaces.
bool Realm::Install() {
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.kit.page);
  JS::RootedObject objectProto(s.cx, JS::GetRealmObjectPrototype(s.cx));
  JS::RootedObject targetProto(s.cx, s.events->prototype());
  if (!objectProto || !targetProto) return false;
  if (!s.kit.Define("RTCPeerConnection", &kPeerClass, targetProto, PeerCreate, &s.peer) ||
      !s.kit.Define("RTCDataChannel", &kChannelClass, targetProto, IllegalCreate, &s.channel) ||
      !s.kit.Define("RTCRtpSender", &kSenderClass, objectProto, IllegalCreate, &s.sender) ||
      !s.kit.Define("RTCRtpReceiver", &kReceiverClass, objectProto, IllegalCreate, &s.receiver) ||
      !s.kit.Define("RTCRtpTransceiver", &kTransceiverClass, objectProto, IllegalCreate,
                    &s.transceiver) ||
      !s.kit.Define("RTCSessionDescription", &kDescriptionClass, objectProto,
                    DescriptionCreate, &s.description) ||
      !s.kit.Define("RTCIceCandidate", &kCandidateClass, objectProto, CandidateCreate,
                    &s.candidate) ||
      !s.kit.Define("RTCCertificate", &kCertificateClass, objectProto, IllegalCreate,
                    &s.certificate))
    return false;
  static const char* const kPeerProps[] = {
      "localDescription", "remoteDescription", "signalingState",
      "iceConnectionState", "iceGatheringState", "connectionState"};
  static const int32_t kPeerPropSlots[] = {
      kPeerLocalDescription, kPeerRemoteDescription, kPeerSignalingState,
      kPeerIceConnectionState, kPeerIceGatheringState, kPeerConnectionState};
  for (size_t i = 0; i < 6; ++i)
    if (!s.kit.Accessor(s.peer, "RTCPeerConnection", kPeerProps[i], PeerGet, nullptr,
                        kPeerPropSlots[i]))
      return false;
  for (int32_t i = 0; i < kPeerHandlerCount; ++i) {
    const std::string name = std::string("on") + kPeerHandlers[i];
    if (!s.kit.Accessor(s.peer, "RTCPeerConnection", name.c_str(), HandlerGet,
                        HandlerSet, HandlerSlot(&kPeerClass, kPeerHandlerBase + i)))
      return false;
  }
  // Firefox exposes getCapabilities as a static WebIDL method on both RTP
  // constructors. The shared implementation keeps the direction in its slot.
  JS::RootedObject senderCapabilities(
      s.cx, s.kit.Function(RtpGetCapabilities, "getCapabilities", 1,
                           kSenderGetCapabilities));
  JS::RootedObject receiverCapabilities(
      s.cx, s.kit.Function(RtpGetCapabilities, "getCapabilities", 1,
                           kReceiverGetCapabilities));
  if (!senderCapabilities || !receiverCapabilities ||
      !JS_DefineProperty(s.cx, s.sender.constructor, "getCapabilities",
                         senderCapabilities, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(s.cx, s.receiver.constructor, "getCapabilities",
                         receiverCapabilities, JSPROP_ENUMERATE) ||
      !s.kit.helpers->Member(senderCapabilities, "RTCRtpSender", "getCapabilities", "call") ||
      !s.kit.helpers->Member(receiverCapabilities, "RTCRtpReceiver", "getCapabilities", "call"))
    return false;
  if (!s.kit.Accessor(s.certificate, "RTCCertificate", "expires", CertificateGet, nullptr,
                      kCertificateExpires) ||
      !s.kit.Method(s.certificate, "RTCCertificate", "getFingerprints", CertificateMethod,
                    0, kCertificateGetFingerprints) ||
      !s.kit.Accessor(s.sender, "RTCRtpSender", "track", SenderGet, nullptr, kSenderTrack) ||
      !s.kit.Accessor(s.receiver, "RTCRtpReceiver", "track", ReceiverGet, nullptr,
                      kReceiverTrack) ||
      !InstallTransceiverMembers(s) ||
      !s.kit.Accessor(s.description, "RTCSessionDescription", "type", DescriptionGet,
                      nullptr, kDescriptionType) ||
      !s.kit.Accessor(s.description, "RTCSessionDescription", "sdp", DescriptionGet,
                      nullptr, kDescriptionSdp) ||
      !s.kit.Method(s.candidate, "RTCIceCandidate", "toJSON", CandidateMethod, 0,
                    kCandidateToJson) ||
      !s.kit.Accessor(s.candidate, "RTCIceCandidate", "candidate", CandidateGet, nullptr,
                      kCandidateText) ||
      !s.kit.Accessor(s.candidate, "RTCIceCandidate", "sdpMid", CandidateGet, nullptr,
                      kCandidateMid) ||
      !s.kit.Accessor(s.candidate, "RTCIceCandidate", "sdpMLineIndex", CandidateGet,
                      nullptr, kCandidateIndex) ||
      !s.kit.Accessor(s.candidate, "RTCIceCandidate", "usernameFragment", CandidateGet,
                      nullptr, kCandidateUser))
    return false;
  // Static WebIDL operation: same function/slot surface as getCapabilities.
  JS::RootedObject generateCertificate(
      s.cx, s.kit.Function(GenerateCertificate, "generateCertificate", 1, 0));
  if (!generateCertificate ||
      !JS_DefineProperty(s.cx, s.peer.constructor, "generateCertificate",
                         generateCertificate, JSPROP_ENUMERATE) ||
      !s.kit.helpers->Member(generateCertificate, "RTCPeerConnection",
                             "generateCertificate", "call"))
    return false;
  static const char* const kPeerMethods[] = {
      "createOffer", "createAnswer", "setLocalDescription", "setRemoteDescription",
      "addIceCandidate", "createDataChannel", "addTrack", "addTransceiver",
      "getSenders", "getReceivers", "getTransceivers", "close"};
  static const unsigned kPeerLengths[] = {0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0};
  for (int32_t i = 0; i <= kPeerClose; ++i)
    if (!s.kit.Method(s.peer, "RTCPeerConnection", kPeerMethods[i], PeerMethod,
                      kPeerLengths[i], i))
      return false;
  return InstallChannelMembers(s) &&
         s.kit.Method(s.description, "RTCSessionDescription", "toJSON", DescriptionMethod, 0,
                      kDescriptionToJson) &&
         InstallTransports(s);
}

// A DedicatedWorker realm (Firefox 158 Exposed=DedicatedWorker members of
// this owner, artifacts/rtc-track): RTCDataChannel (transferable, created
// only by a peer, so never constructed here), RTCError, the encoded frames
// and RTCRtpScriptTransformer. No peer connection exists in a worker.
bool Realm::InstallWorker() {
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.kit.page);
  JS::RootedObject objectProto(s.cx, JS::GetRealmObjectPrototype(s.cx));
  JS::RootedObject targetProto(s.cx, s.events->prototype());
  return objectProto && targetProto &&
         s.kit.Define("RTCDataChannel", &kChannelClass, targetProto, IllegalCreate, &s.channel) &&
         InstallChannelMembers(s) && InstallError(s) && InstallEncodedFrames(s, objectProto) &&
         InstallScriptTransformer(s, objectProto);
}

}  // namespace ruyijs::web_rtc
