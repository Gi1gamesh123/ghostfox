#ifndef RUYIJS_WEB_RTC_STATE_H
#define RUYIJS_WEB_RTC_STATE_H
#include <cstdint>
#include <string>
#include "interaction/realm.h"
#include "js/GCVector.h"
#include "js/RootingAPI.h"
#include "jsapi.h"
#include "runtime/web_kit.h"
#include "web_rtc/model.h"
#include "web_rtc/realm.h"

namespace ruyijs::web_rtc {

// Handler IDL attributes per interface, in slot order.
inline constexpr int32_t kPeerHandlerCount = 9;
inline constexpr int32_t kChannelHandlerCount = 6;

// Every RTC object keeps its observable state in reserved slots, so the object
// graph owns the values and no C++ side table can outlive or shadow them. The
// aggregate (Model) owns peer/channel/transceiver state; slots hold the
// realm-local view and the stable JS identities the spec exposes.
enum PeerSlot : int32_t {
  kPeerId = 0,               // domain peer id (0 = detached)
  kPeerLocalDescription = 1, // RTCSessionDescription | null
  kPeerRemoteDescription = 2,
  kPeerSignalingState = 3,
  kPeerIceConnectionState = 4,
  kPeerIceGatheringState = 5,
  kPeerConnectionState = 6,
  kPeerSenders = 7,          // JS array, stable identity for getSenders()
  kPeerReceivers = 8,
  kPeerTransceivers = 9,
  kPeerChannels = 10,
  kPeerHandlerBase = 11,     // one slot per on* handler, see kPeerHandlers
  kPeerSlotCount = kPeerHandlerBase + kPeerHandlerCount,
};

enum ChannelSlot : int32_t {
  kChannelId = 0,
  kChannelLabel = 1,
  kChannelOrdered = 2,
  kChannelMaxRetransmits = 3,
  kChannelProtocol = 4,
  kChannelNegotiated = 5,
  kChannelNegotiatedId = 6,
  kChannelReadyState = 7,
  kChannelBuffered = 8,
  kChannelPeer = 9,          // owning RTCPeerConnection
  kChannelMaxPacketLifeTime = 10,  // unsigned short or null
  kChannelLowThreshold = 11,       // bufferedAmountLowThreshold (unsigned long)
  kChannelBinaryType = 12,         // "arraybuffer" (default) or "blob"
  kChannelHandlerBase = 13,  // see kChannelHandlers
  kChannelSlotCount = kChannelHandlerBase + kChannelHandlerCount,
};

enum SenderSlot : int32_t { kSenderPeer = 0, kSenderKind = 1, kSenderTrack = 2,
                            kSenderSlotCount = 3 };
enum ReceiverSlot : int32_t { kReceiverPeer = 0, kReceiverKind = 1,
                              kReceiverTrack = 2, kReceiverSlotCount = 3 };
enum TransceiverSlot : int32_t {
  kTransceiverId = 0, kTransceiverPeer = 1, kTransceiverMid = 2,
  kTransceiverDirection = 3, kTransceiverKind = 4, kTransceiverSender = 5,
  kTransceiverReceiver = 6, kTransceiverSlotCount = 7,
};
enum DescriptionSlot : int32_t { kDescriptionType = 0, kDescriptionSdp = 1,
                                 kDescriptionSlotCount = 2 };
enum CandidateSlot : int32_t {
  kCandidateText = 0, kCandidateMid = 1, kCandidateIndex = 2, kCandidateUser = 3,
  kCandidateSlotCount = 4,
};
// RTCCertificate holds the value RTCPeerConnection.generateCertificate
// resolved with: an expiry stamp and the DTLS fingerprint of its certificate.
enum CertificateSlot : int32_t {
  kCertificateExpires = 0, kCertificateFingerprint = 1, kCertificateSlotCount = 2,
};

// Transport, stats and script-transform interfaces (transports.cpp). The
// transports are EventTargets whose on* handlers follow their state slots.
inline constexpr int32_t kDtlsHandlerCount = 2;
inline constexpr int32_t kIceHandlerCount = 3;
inline constexpr int32_t kSctpHandlerCount = 1;
enum DtlsSlot : int32_t {
  kDtlsIceTransport = 0, kDtlsState = 1, kDtlsHandlerBase = 2,
  kDtlsSlotCount = kDtlsHandlerBase + kDtlsHandlerCount,
};
enum IceSlot : int32_t {
  kIceRole = 0, kIceState = 1, kIceGatheringState = 2, kIceHandlerBase = 3,
  kIceSlotCount = kIceHandlerBase + kIceHandlerCount,
};
enum SctpSlot : int32_t {
  kSctpTransport = 0, kSctpState = 1, kSctpMaxMessageSize = 2, kSctpMaxChannels = 3,
  kSctpHandlerBase = 4, kSctpSlotCount = kSctpHandlerBase + kSctpHandlerCount,
};
enum StatsReportSlot : int32_t { kStatsMap = 0, kStatsSlotCount = 1 };
enum ScriptTransformSlot : int32_t {
  kScriptWorker = 0, kScriptOptions = 1, kScriptSlotCount = 2,
};
enum TransportMethodId : int32_t {
  kDtlsGetRemoteCertificates = 0, kIceGetSelectedCandidatePair = 1,
};
// Read-only maplike operations, in Firefox's prototype order.
enum StatsMethodId : int32_t {
  kStatsEntries = 0, kStatsKeys, kStatsValues, kStatsForEach, kStatsHas, kStatsGet,
  kStatsSize,
};
extern const char* const kDtlsHandlers[];
extern const char* const kIceHandlers[];
extern const char* const kSctpHandlers[];

// The event-handler IDL attributes, in slot order after the *HandlerBase.
extern const char* const kPeerHandlers[];
extern const char* const kChannelHandlers[];

struct State {
  JSContext* cx;
  WebKit kit;
  Model model;
  interaction::Realm* events = nullptr;
  WebFace peer, channel, sender, receiver, transceiver, description, candidate,
      certificate, dtls, ice, sctp, stats, scriptTransform, encodedAudio, encodedVideo, scriptTransformer, error;
  Realm::WorkerCheckPort workerCheck = nullptr;
  Realm::ExceptionPorts exceptions;
  // Delayed task port (ICE gathering steps), bound by the runtime coordinator.
  bool (*postTask)(void*, JS::HandleObject, double) = nullptr;
  void* postTaskContext = nullptr;
  Realm::RemoteTrackPort remoteTrack = nullptr;
  void* remoteTrackContext = nullptr;
  State(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
        interaction::Realm& events);
};

extern const JSClass kPeerClass, kChannelClass, kSenderClass, kReceiverClass,
    kTransceiverClass, kDescriptionClass, kCandidateClass, kCertificateClass;
extern const JSClass kDtlsClass, kIceClass, kSctpClass, kStatsClass, kScriptTransformClass;
// Installs the five interfaces above; called last by Realm::Install.
bool InstallTransports(State& state);
// RTCDTMFSender and RTCIceCandidatePair, interface objects only (negotiation_views.cpp).
bool InstallNegotiationViews(State& state);
// RTCStatsReport (stats_report.cpp); called by InstallTransports in Firefox order.
bool InstallStatsReport(State& state, JS::HandleObject objectProto);
// RTCEncodedAudioFrame/VideoFrame (encoded_frame.cpp); after RTCRtpScriptTransform.
bool InstallEncodedFrames(State& state, JS::HandleObject objectProto);
// RTCDataChannel members (channel.cpp); RTCRtpScriptTransformer (script_transformer.cpp, workers).
bool InstallChannelMembers(State& state);
bool InstallScriptTransformer(State& state, JS::HandleObject objectProto);
// RTCError (error.cpp): a DOMException subclass over the exception ports.
bool InstallError(State& state);

// Method dispatch ids, passed through the shared WebKit method slot.
enum PeerMethodId : int32_t {
  kPeerCreateOffer = 0, kPeerCreateAnswer, kPeerSetLocal, kPeerSetRemote,
  kPeerAddIceCandidate, kPeerCreateDataChannel, kPeerAddTrack, kPeerAddTransceiver,
  kPeerGetSenders, kPeerGetReceivers, kPeerGetTransceivers, kPeerClose,
};
enum ChannelMethodId : int32_t { kChannelSend = 0, kChannelClose };
enum DescriptionMethodId : int32_t { kDescriptionToJson = 0 };
enum CandidateMethodId : int32_t { kCandidateToJson = 0 };
enum CertificateMethodId : int32_t { kCertificateGetFingerprints = 0 };

// Static capability methods installed on the two RTP constructors.
enum RtpCapabilityMethodId : int32_t {
  kSenderGetCapabilities = 0,
  kReceiverGetCapabilities = 1,
};

bool PeerCreate(JSContext*, unsigned, JS::Value*);
bool DescriptionCreate(JSContext*, unsigned, JS::Value*);
bool CandidateCreate(JSContext*, unsigned, JS::Value*);
bool IllegalCreate(JSContext*, unsigned, JS::Value*);
bool PeerGet(JSContext*, unsigned, JS::Value*);
bool ChannelGet(JSContext*, unsigned, JS::Value*);
bool ChannelSet(JSContext*, unsigned, JS::Value*);  // binaryType, bufferedAmountLowThreshold
bool SenderGet(JSContext*, unsigned, JS::Value*);
bool ReceiverGet(JSContext*, unsigned, JS::Value*);
bool TransceiverGet(JSContext*, unsigned, JS::Value*);
bool DescriptionGet(JSContext*, unsigned, JS::Value*);
bool CertificateGet(JSContext*, unsigned, JS::Value*);
bool CandidateGet(JSContext*, unsigned, JS::Value*);
bool RtpGetCapabilities(JSContext*, unsigned, JS::Value*);
// transceiver.cpp: RTCRtpTransceiver's members in Firefox prototype order.
bool InstallTransceiverMembers(State& state);
bool HandlerGet(JSContext*, unsigned, JS::Value*);
bool HandlerSet(JSContext*, unsigned, JS::Value*);
bool PeerMethod(JSContext*, unsigned, JS::Value*);
bool ChannelMethod(JSContext*, unsigned, JS::Value*);
bool DescriptionMethod(JSContext*, unsigned, JS::Value*);
bool CandidateMethod(JSContext*, unsigned, JS::Value*);
bool CertificateMethod(JSContext*, unsigned, JS::Value*);
// Static RTCPeerConnection.generateCertificate; keygen and certificate
// generation are pending, the promise resolves with an RTCCertificate.
bool GenerateCertificate(JSContext*, unsigned, JS::Value*);

// One error factory for the domain: the page realm's own DOMException (or
// TypeError), never a second error type. Throw* sets the pending exception;
// Reject* settles a promise, for the promise-returning operations.
bool MakeException(State& state, const char* interface, const char* name,
                   const char* message, JS::MutableHandleValue result);
// WebIDL brand check: on a foreign receiver throws Firefox's
// "'<member>' called on an object that does not implement interface <I>.",
// the member text being the callee's own name ("get x", "set x", "method").
bool Brand(JSContext* cx, const JS::CallArgs& args, const JSClass* clasp,
           JS::MutableHandleObject self);
// Shared on* accessors carry their interface in the slot: (index << 8) | slot.
int32_t HandlerSlot(const JSClass* clasp, int32_t slot);
bool ThrowError(State& state, const char* interface, const char* name,
                const char* message);
bool RejectError(State& state, const char* interface, const char* name,
                 const char* message, JS::MutableHandleValue result);

// Realm object factories. Each one creates the object, fills its slots from the
// aggregate and attaches it to the shared event tree.
JSObject* NewPeer(State& state, uint32_t id, JS::HandleObject proto);
JSObject* NewChannel(State& state, JS::HandleObject peer, uint32_t id);
JSObject* NewSender(State& state, uint32_t peer, const char* kind);
JSObject* NewReceiver(State& state, uint32_t peer, const char* kind);
JSObject* NewTransceiver(State& state, JS::HandleObject owner, uint32_t id);
JSObject* NewDescription(State& state, const DescriptionSpec& spec, JS::HandleObject proto);
JSObject* NewCandidate(State& state, JS::HandleValue dictionary, JS::HandleObject proto);
JSObject* NewCertificate(State& state, double expires, const std::string& fingerprint,
                         JS::HandleObject proto);

// Re-reads the aggregate into the peer's string slots and fires one change
// event per field that moved. Returns false only on OOM.
bool SyncPeer(State& state, JS::HandleObject peer);
// ice_gathering.cpp: trickle host candidates for a new local description.
bool StartGathering(State& state, JS::HandleObject peer, const std::string& sdp);
// Re-reads the peer's channels, firing open/close for each transition.
bool SyncChannels(State& state, JS::HandleObject peer);

// Appends value to an already-created JS array slot (stable identity).
bool Append(JSContext* cx, JS::HandleObject array, JS::HandleObject value);

}  // namespace ruyijs::web_rtc
#endif
