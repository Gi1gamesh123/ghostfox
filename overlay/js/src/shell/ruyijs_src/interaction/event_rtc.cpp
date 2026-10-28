// The three T3c WebRTC payload events: RTCPeerConnectionIceEvent,
// RTCDataChannelEvent and RTCTrackEvent (dom/webidl/RTCPeerConnectionIceEvent
// .webidl, RTCDataChannelEvent.webidl, RTCTrackEvent.webidl), plus
// RTCTransformEvent (dom/webidl/RTCTransformEvent.webidl, DedicatedWorker only).
// The referenced interfaces (RTCIceCandidate, RTCDataChannel, RTCRtpReceiver,
// MediaStreamTrack, RTCRtpTransceiver, MediaStream) belong to the web_rtc
// domain, so the dictionary values are carried verbatim; streams is the one
// sequence member and is materialized as an ordinary array. The payload tables
// stay in their own TU so no table TU grows past its split budget.
#include "interaction/internal.h"
#include "js/Array.h"
#include "js/ForOfIterator.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include <cstdio>
#include <chrono>

namespace ruyijs::interaction {
namespace {
JSObject* Self(const JS::CallArgs& args) { return &args.thisv().toObject(); }
bool Slot(JSContext* cx, const JS::CallArgs& args, const char* member, Iface iface,
          uint32_t slot) {
  Event* event = Receiver(cx, args, iface);
  if (!event) return false;
  ReportEventAccess(cx, event, InterfaceName(iface), member, int(ApiAccess::Read));
  args.rval().set(JS::GetReservedSlot(Self(args), slot));
  return JS_WrapValue(cx, args.rval());
}
#define RTC_GETTER(name, member, iface, slot)                             \
  bool name(JSContext* cx, unsigned argc, JS::Value* vp) {                \
    return Slot(cx, JS::CallArgsFromVp(argc, vp), member, iface, slot);   \
  }
RTC_GETTER(GetIceCandidate, "candidate", Iface::RtcIce, kEventIceCandidate)
RTC_GETTER(GetDataChannel, "channel", Iface::RtcDataChannel, kEventChannel)
RTC_GETTER(GetTrackReceiver, "receiver", Iface::RtcTrack, kEventTrackReceiver)
RTC_GETTER(GetTrackTrack, "track", Iface::RtcTrack, kEventTrack)
RTC_GETTER(GetTrackStreams, "streams", Iface::RtcTrack, kEventTrackStreams)
RTC_GETTER(GetTrackTransceiver, "transceiver", Iface::RtcTrack,
           kEventTrackTransceiver)
// RTCTransformEvent.transformer: the Firefox payload factory's first public slot.
RTC_GETTER(GetTransformer, "transformer", Iface::RtcTransform, kEventPublicFirst)
#undef RTC_GETTER

// RTCTrackEventInit.streams is a sequence<MediaStream>: undefined keeps the
// default [], any other value must convert through the WebIDL sequence rule
// (the donor rejects a non-iterable with MSG_CONVERSION_ERROR and a primitive
// element with the MediaStream interface error,
// obj-jscall-check/dom/bindings/RTCTrackEventBinding.cpp:138-215), and the
// accepted elements are carried verbatim. The converted array is not frozen:
// the donor's getter caches and freezes its own copy (RTCTrackEventBinding.cpp
// :345-432); this runtime stores the array the constructor produced.
bool ReadStreams(JSContext* cx, JS::HandleObject dict, JS::HandleObject array) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, dict, "streams", &value)) return false;
  if (value.isUndefined()) return true;
  if (!value.isObject())
    return Error(cx, "TypeError",
                 "Event dictionary member 'streams' requires a sequence");
  JS::ForOfIterator iterator(cx);
  if (!iterator.init(value, JS::ForOfIterator::AllowNonIterable)) return false;
  if (!iterator.valueIsIterable())
    return Error(cx, "TypeError",
                 "Event dictionary member 'streams' requires a sequence");
  JS::RootedValue element(cx);
  uint32_t index = 0;
  while (true) {
    bool done = false;
    if (!iterator.next(&element, &done)) return false;
    if (done) break;
    if (!element.isObject())
      return Error(cx, "TypeError",
                   "Event dictionary member 'streams' requires objects");
    if (!JS_DefineElement(cx, array, index, element, JSPROP_ENUMERATE))
      return false;
    ++index;
  }
  return true;
}
}  // namespace

extern const JSPropertySpec kRtcIceProperties[] = {
  JS_PSG("candidate", GetIceCandidate, JSPROP_ENUMERATE),
  JS_PS_END
};
extern const JSPropertySpec kRtcDataChannelProperties[] = {
  JS_PSG("channel", GetDataChannel, JSPROP_ENUMERATE),
  JS_PS_END
};
extern const JSPropertySpec kRtcTrackProperties[] = {
  JS_PSG("receiver", GetTrackReceiver, JSPROP_ENUMERATE),
  JS_PSG("track", GetTrackTrack, JSPROP_ENUMERATE),
  JS_PSG("streams", GetTrackStreams, JSPROP_ENUMERATE),
  JS_PSG("transceiver", GetTrackTransceiver, JSPROP_ENUMERATE),
  JS_PS_END
};

extern const JSPropertySpec kRtcTransformProperties[] = {
  JS_PSG("transformer", GetTransformer, JSPROP_ENUMERATE),
  JS_PS_END
};

bool InitRtcFields(JSContext* cx, Iface iface, JS::HandleObject dict,
                   JS::HandleObject object) {
  switch (iface) {
    case Iface::RtcIce: {
      JS::RootedValue candidate(cx);
      if (!ReadNullableObject(cx, dict, "candidate", &candidate)) return false;
      JS::SetReservedSlot(object, kEventIceCandidate, candidate);
      return true;
    }
    case Iface::RtcDataChannel: {
      JS::RootedValue channel(cx);
      if (!ReadRequiredObject(cx, dict, "channel", &channel)) return false;
      JS::SetReservedSlot(object, kEventChannel, channel);
      return true;
    }
    case Iface::RtcTrack: {
      JS::RootedValue receiver(cx), track(cx), transceiver(cx);
      if (!ReadRequiredObject(cx, dict, "receiver", &receiver) ||
          !ReadRequiredObject(cx, dict, "track", &track) ||
          !ReadRequiredObject(cx, dict, "transceiver", &transceiver)) return false;
      JS::RootedObject streams(cx, JS::NewArrayObject(cx, 0));
      if (!streams) return false;
      if (dict && !ReadStreams(cx, dict, streams)) return false;
      JS::SetReservedSlot(object, kEventTrackReceiver, receiver);
      JS::SetReservedSlot(object, kEventTrack, track);
      JS::SetReservedSlot(object, kEventTrackStreams, JS::ObjectValue(*streams));
      JS::SetReservedSlot(object, kEventTrackTransceiver, transceiver);
      return true;
    }
    default:
      return true;
  }
}
}  // namespace ruyijs::interaction
