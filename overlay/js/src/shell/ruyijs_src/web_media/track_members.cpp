// MediaStreamTrack members in Firefox 158 prototype order (clone, stop,
// getCapabilities, getConstraints, getSettings, applyConstraints, kind, id,
// label, enabled, muted, onmute, onunmute, readyState, onended). Values and
// texts: artifacts/rtc-track, artifacts/media-track (remote and canvas tracks).
#include "web_media/track.h"
#include "web_media/stream.h"
#include "web_media/media_runtime.h"
#include "jsapi.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::web_media {
namespace {
enum Member { Clone, Stop, Capabilities, Constraints, Settings, Apply, Kind, Id, Label, Enabled,
              Muted, OnMute, OnUnmute, ReadyState, OnEnded };
constexpr const char* kMethods[] = {"clone", "stop", "getCapabilities", "getConstraints", "getSettings",
                                    "applyConstraints"};
constexpr const char* kFields[] = {"kind", "id", "label", "enabled", "muted", "onmute", "onunmute",
                                   "readyState", "onended"};
bool Receiver(JSContext* cx, const JS::CallArgs& args, JS::MutableHandleObject self, State** owner) {
  self.set(args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!IsTrack(self)) return WebKit::FailBrand(cx, args, "MediaStreamTrack");
  *owner = static_cast<State*>(JS::GetReservedSlot(self, MSTrackOwner).toPrivate());
  return true;
}
// MediaTrackSettings: a remote audio track reports {}, a remote video track
// {height: 0, width: 0}, a canvas track the canvas's current size.
bool SettingsOf(JSContext* cx, State& s, JS::HandleObject self, JS::MutableHandleValue out) {
  JS::RootedObject result(cx, JS_NewPlainObject(cx));
  if (!result) return false;
  if (TrackIsVideo(self)) {
    uint32_t width = 0, height = 0;
    JS::RootedValue canvas(cx, JS::GetReservedSlot(self, MSTrackCanvas));
    if (canvas.isObject()) {
      JS::RootedObject element(cx, &canvas.toObject());
      if (!s.canvasSize) return WebKit::Fail(cx, "MediaStreamTrack canvas size port is not installed");
      if (!s.canvasSize(s.canvasSizeContext, element, &width, &height)) return false;
    }
    if (!JS_DefineProperty(cx, result, "height", height, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, result, "width", width, JSPROP_ENUMERATE)) return false;
  }
  out.setObject(*result);
  return true;
}
bool TrackMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int member = WebKit::Slot(args);
  JS::RootedObject self(cx), result(cx);
  State* s = nullptr;
  if (!Receiver(cx, args, &self, &s)) return false;
  switch (member) {
    case Clone: {
      {
        JSAutoRealm realm(cx, s->kit.page);
        result = CloneTrack(*s, self);
      }
      if (!result || !JS_WrapObject(cx, &result)) return false;
      args.rval().setObject(*result);
      return true;
    }
    case Stop:  // Firefox ends the track without firing `ended` at it.
      JS::SetReservedSlot(self, MSTrackEnded, JS::TrueValue());
      args.rval().setUndefined();
      return true;
    case Settings: return SettingsOf(cx, *s, self, args.rval());
    default: {  // getCapabilities/getConstraints: no capability or constraint applies.
      result = JS_NewPlainObject(cx);
      if (!result) return false;
      args.rval().setObject(*result);
      return true;
    }
  }
}
bool TrackGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int member = WebKit::Slot(args);
  JS::RootedObject self(cx);
  State* s = nullptr;
  if (!Receiver(cx, args, &self, &s)) return false;
  int slot = 0;
  switch (member) {
    case Kind: slot = MSTrackKind; break;
    case Id: slot = MSTrackId; break;
    case Label: slot = MSTrackLabel; break;
    case Enabled: slot = MSTrackEnabled; break;
    case Muted: slot = MSTrackMuted; break;
    case OnMute: slot = MSTrackOnMute; break;
    case OnUnmute: slot = MSTrackOnUnmute; break;
    case OnEnded: slot = MSTrackOnEnded; break;
    default: {
      JSString* text = JS_NewStringCopyZ(cx, TrackIsLive(self) ? "live" : "ended");
      if (!text) return false;
      args.rval().setString(text);
      return true;
    }
  }
  args.rval().set(JS::GetReservedSlot(self, slot));
  return JS_WrapValue(cx, args.rval());
}
bool TrackSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int member = WebKit::Slot(args);
  JS::RootedObject self(cx);
  State* s = nullptr;
  if (!Receiver(cx, args, &self, &s)) return false;
  JS::RootedValue value(cx, args.get(0));
  args.rval().setUndefined();
  if (member == Enabled) {
    JS::SetReservedSlot(self, MSTrackEnabled, JS::BooleanValue(JS::ToBoolean(value)));
    return true;
  }
  const EventHandlerSlots slots =
      member == OnMute ? EventHandlerSlots{MSTrackOnMute, MSTrackMuteListener, MSTrackOwner, "mute"} :
      member == OnUnmute ? EventHandlerSlots{MSTrackOnUnmute, MSTrackUnmuteListener, MSTrackOwner, "unmute"} :
                           EventHandlerSlots{MSTrackOnEnded, MSTrackEndedListener, MSTrackOwner, "ended"};
  return SetEventHandler(*s, self, slots, value);
}
// applyConstraints(): the dictionary converts first (a non-object rejects with
// TypeError); neither source can satisfy a constraint set, so every request
// rejects with Firefox's OverconstrainedError MediaStreamError.
bool ApplyConstraints(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject caller(cx, JS::CurrentGlobalOrNull(cx)), self(cx);
  State* s = nullptr;
  if (!Receiver(cx, args, &self, &s)) return RejectMediaException(cx, caller, args.rval());
  if (!args.get(0).isNullOrUndefined() && !args.get(0).isObject()) {
    WebKit::Fail(cx, "MediaStreamTrack.applyConstraints: Value can't be converted to a dictionary.");
    return RejectMediaException(cx, caller, args.rval());
  }
  JS::RootedObject error(cx);
  if (!NewMediaStreamError(*s, "OverconstrainedError", "Constraints could not be satisfied.", "", &error))
    return false;
  JS::RootedValue reason(cx, JS::ObjectValue(*error));
  if (!JS_WrapValue(cx, &reason)) return false;
  JS_SetPendingException(cx, reason);
  return RejectMediaException(cx, caller, args.rval());
}
}  // namespace

bool InstallTrack(State& s, JS::HandleObject eventTargetConstructor) {
  JS::RootedObject parent(s.cx, s.events.prototype());
  if (!s.kit.Define("MediaStreamTrack", &kTrackClass, parent, TrackConstructor, &s.track) ||
      !JS_SetPrototype(s.cx, s.track.constructor, eventTargetConstructor)) return false;
  for (int i = Clone; i <= Apply; ++i)
    if (!s.kit.Method(s.track, "MediaStreamTrack", kMethods[i], i == Apply ? ApplyConstraints : TrackMethod,
                      0, i)) return false;
  for (int i = Kind; i <= OnEnded; ++i) {
    const bool settable = i == Enabled || i == OnMute || i == OnUnmute || i == OnEnded;
    if (!s.kit.Accessor(s.track, "MediaStreamTrack", kFields[i - Kind], TrackGet,
                        settable ? TrackSet : nullptr, i)) return false;
  }
  return MediaConstructorLast(s, s.track);
}
}  // namespace ruyijs::web_media
