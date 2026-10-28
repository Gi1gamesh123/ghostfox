#include "web_rtc/state.h"

#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "js/ValueArray.h"
#include "jsfriendapi.h"
#include "runtime/runtime.h"

#include <string>

namespace ruyijs::web_rtc {

// The handler IDL attributes are the only on* accessors; they delegate to the
// shared EventTarget methods so a handler and addEventListener(listener) are
// the same registration.
const char* const kPeerHandlers[] = {
    "signalingstatechange", "iceconnectionstatechange", "icegatheringstatechange",
    "connectionstatechange", "icecandidate", "icecandidateerror", "datachannel",
    "track", "negotiationneeded"};
const char* const kChannelHandlers[] = {
    "open", "message", "close", "error", "closing", "bufferedamountlow"};
const char* const kDtlsHandlers[] = {"statechange", "error"};
const char* const kIceHandlers[] = {"statechange", "gatheringstatechange",
                                    "selectedcandidatepairchange"};
const char* const kSctpHandlers[] = {"statechange"};

const JSClass kPeerClass = {"RTCPeerConnection",
                            JSCLASS_HAS_RESERVED_SLOTS(kPeerSlotCount)};
const JSClass kChannelClass = {"RTCDataChannel",
                               JSCLASS_HAS_RESERVED_SLOTS(kChannelSlotCount)};
const JSClass kSenderClass = {"RTCRtpSender",
                              JSCLASS_HAS_RESERVED_SLOTS(kSenderSlotCount)};
const JSClass kReceiverClass = {"RTCRtpReceiver",
                                JSCLASS_HAS_RESERVED_SLOTS(kReceiverSlotCount)};
const JSClass kTransceiverClass = {"RTCRtpTransceiver",
                                   JSCLASS_HAS_RESERVED_SLOTS(kTransceiverSlotCount)};
const JSClass kDescriptionClass = {"RTCSessionDescription",
                                   JSCLASS_HAS_RESERVED_SLOTS(kDescriptionSlotCount)};
const JSClass kCandidateClass = {"RTCIceCandidate",
                                 JSCLASS_HAS_RESERVED_SLOTS(kCandidateSlotCount)};
const JSClass kCertificateClass = {"RTCCertificate",
                                   JSCLASS_HAS_RESERVED_SLOTS(kCertificateSlotCount)};

State::State(JSContext* context, JS::HandleObject page, PageHelpers& helpers,
             interaction::Realm& events_)
    : cx(context),
      kit(context, page, helpers, this),
      model(),
      events(&events_),
      peer(context),
      channel(context),
      sender(context),
      receiver(context),
      transceiver(context),
      description(context),
      candidate(context),
      certificate(context),
      dtls(context),
      ice(context),
      sctp(context),
      stats(context),
      scriptTransform(context),
      encodedAudio(context),
      encodedVideo(context),
      scriptTransformer(context),
      error(context) {}

Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
             interaction::Realm& events)
    : state_(std::make_unique<State>(cx, page, helpers, events)) {}
Realm::~Realm() = default;
void Realm::SetWorkerCheck(WorkerCheckPort port) { state_->workerCheck = port; }
void Realm::SetExceptionPorts(const ExceptionPorts& ports) { state_->exceptions = ports; }
void Realm::SetRemoteTrackPort(RemoteTrackPort port, void* context) {
  state_->remoteTrack = port;
  state_->remoteTrackContext = context;
}
void Realm::SetTaskPort(bool (*port)(void*, JS::HandleObject, double), void* context) {
  state_->postTask = port;
  state_->postTaskContext = context;
}

// RTCRtpSender/Receiver/Transceiver and RTCDataChannel are created only by the
// owning peer, so their globals are not constructible.
bool IllegalCreate(JSContext* cx, unsigned, JS::Value*) {
  return WebKit::Fail(cx, "Illegal constructor.");
}

// The one error value factory: the value comes from the page realm's own
// DOMException/TypeError interface, so WebRTC owns no second error type.
bool MakeException(State& state, const char* interface, const char* name,
                   const char* message, JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  JS::RootedValue constructor(cx);
  if (!JS_GetProperty(cx, state.kit.page, interface, &constructor) ||
      !constructor.isObject())
    return WebKit::Fail(cx, "WebRTC requires the page error interfaces");
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, message ? message : ""));
  JS::RootedValueArray<2> arguments(cx);
  if (!text) return false;
  arguments[0].setString(text);
  if (name) {
    JS::RootedString label(cx, JS_NewStringCopyZ(cx, name));
    if (!label) return false;
    arguments[1].setString(label);
  } else {
    arguments[1].setUndefined();
  }
  JS::RootedObject error(cx);
  if (!JS::Construct(cx, constructor, arguments, &error)) return false;
  result.setObject(*error);
  return true;
}

bool ThrowError(State& state, const char* interface, const char* name,
                const char* message) {
  JS::RootedValue error(state.cx);
  if (!MakeException(state, interface, name, message, &error)) return false;
  JS_SetPendingException(state.cx, error);
  return false;
}

bool RejectError(State& state, const char* interface, const char* name,
                 const char* message, JS::MutableHandleValue result) {
  JS::RootedValue error(state.cx);
  if (!MakeException(state, interface, name, message, &error)) return false;
  return state.kit.RejectWith(error, result);
}

bool Append(JSContext* cx, JS::HandleObject array, JS::HandleObject value) {
  uint32_t length = 0;
  JS::RootedValue element(cx, JS::ObjectValue(*value));
  return JS::GetArrayLength(cx, array, &length) &&
         JS_DefineElement(cx, array, length, element, JSPROP_ENUMERATE);
}

}  // namespace ruyijs::web_rtc
