// The one MediaStreamTrack factory of the web_media owner. A track is an
// EventTarget whose kind, id, label, enabled/muted/ended state and source live
// in its reserved slots (track.h). Sources: an RTCRtpReceiver's remote track
// (Realm::NewRemoteTrack, label "remote audio"/"remote video", muted) and a
// canvas capture (canvas_capture.cpp, label "", unmuted). Firefox 158 values:
// artifacts/rtc-track and artifacts/media-track.
#include "web_media/track.h"
#include "web_media/realm.h"
#include "web_media/stream.h"
#include "runtime/native_constructor_trace.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::web_media {
const JSClass kTrackClass = {"MediaStreamTrack", JSCLASS_HAS_RESERVED_SLOTS(MSTrackSlotCount)};

bool IsTrack(JSObject* object) { return object && JS::GetClass(object) == &kTrackClass; }
bool TrackIsVideo(JSObject* track) { return JS::GetReservedSlot(track, MSTrackVideo).toBoolean(); }
bool TrackIsLive(JSObject* track) { return !JS::GetReservedSlot(track, MSTrackEnded).toBoolean(); }
bool IsMediaStreamTrack(JSObject* object) {
  return IsTrack(object ? js::CheckedUnwrapStatic(object) : nullptr);
}

JSObject* NewTrack(State& s, const TrackSource& source) {
  JSContext* cx = s.cx;
  JS::RootedObject track(cx, s.kit.New(&kTrackClass, s.track.prototype));
  JS::RootedString id(cx, NewMediaId(s));
  JS::RootedString kind(cx, JS_NewStringCopyZ(cx, source.video ? "video" : "audio"));
  JS::RootedString label(cx, JS_NewStringCopyZ(cx, source.label));
  if (!track || !id || !kind || !label) return nullptr;
  JS::SetReservedSlot(track, MSTrackOwner, JS::PrivateValue(&s));
  JS::SetReservedSlot(track, MSTrackVideo, JS::BooleanValue(source.video));
  JS::SetReservedSlot(track, MSTrackKind, JS::StringValue(kind));
  JS::SetReservedSlot(track, MSTrackId, JS::StringValue(id));
  JS::SetReservedSlot(track, MSTrackLabel, JS::StringValue(label));
  JS::SetReservedSlot(track, MSTrackEnabled, JS::TrueValue());
  JS::SetReservedSlot(track, MSTrackMuted, JS::BooleanValue(source.muted));
  JS::SetReservedSlot(track, MSTrackEnded, JS::FalseValue());
  JS::SetReservedSlot(track, MSTrackOrigin, JS::Int32Value(int32_t(source.origin)));
  JS::SetReservedSlot(track, MSTrackCanvas, source.canvas ? JS::ObjectValue(*source.canvas) : JS::NullValue());
  for (int slot : {MSTrackOnMute, MSTrackOnUnmute, MSTrackOnEnded, MSTrackMuteListener, MSTrackUnmuteListener,
                   MSTrackEndedListener})
    JS::SetReservedSlot(track, slot, JS::NullValue());
  return s.events.Attach(track) ? track.get() : nullptr;
}

// Firefox MediaStreamTrack::Clone: a new id over the same source, carrying the
// enabled, muted and ended state; handlers and listeners are not copied.
JSObject* CloneTrack(State& s, JS::HandleObject track) {
  JSContext* cx = s.cx;
  JS::RootedObject canvas(cx);
  JS::RootedValue source(cx, JS::GetReservedSlot(track, MSTrackCanvas));
  if (source.isObject()) canvas = &source.toObject();
  JS::RootedString label(cx, JS::GetReservedSlot(track, MSTrackLabel).toString());
  JS::UniqueChars text = JS_EncodeStringToUTF8(cx, label);
  if (!text) return nullptr;
  TrackSource init;
  init.video = TrackIsVideo(track);
  init.origin = Origin(JS::GetReservedSlot(track, MSTrackOrigin).toInt32());
  init.label = text.get();
  init.muted = JS::GetReservedSlot(track, MSTrackMuted).toBoolean();
  init.canvas = canvas;
  JS::RootedObject copy(cx, NewTrack(s, init));
  if (!copy) return nullptr;
  JS::SetReservedSlot(copy, MSTrackEnabled, JS::GetReservedSlot(track, MSTrackEnabled));
  JS::SetReservedSlot(copy, MSTrackEnded, JS::GetReservedSlot(track, MSTrackEnded));
  return copy;
}

bool Realm::NewRemoteTrack(bool video, JS::MutableHandleObject track) {
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.kit.page);
  TrackSource init;
  init.video = video;
  init.origin = Origin::Remote;
  init.label = video ? "remote video" : "remote audio";
  init.muted = true;
  track.set(NewTrack(s, init));
  return track != nullptr;
}

bool TrackConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return WebKit::Fail(cx, "Illegal constructor");
}
}  // namespace ruyijs::web_media
