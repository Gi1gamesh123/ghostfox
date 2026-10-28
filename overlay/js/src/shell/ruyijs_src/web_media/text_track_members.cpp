#include "web_media/text_track.h"
#include "web_media/media_runtime.h"
#include "web_media/state.h"
#include "jsapi.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
#include <cmath>

namespace ruyijs::web_media {
namespace {
bool TrackReceiver(JSContext* cx, JS::HandleValue value, JS::MutableHandleObject out) {
  out.set(value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  return (out && JS::GetClass(out) == &kTextTrackClass) ||
      WebKit::Fail(cx, "Illegal TextTrack receiver");
}
bool CueReceiver(JSContext* cx, JS::HandleValue value, JS::MutableHandleObject out) {
  out.set(value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  return (out && (JS::GetClass(out) == &kTextTrackCueClass ||
                  JS::GetClass(out) == &kVTTCueClass)) ||
      WebKit::Fail(cx, "Illegal TextTrackCue receiver");
}
bool ListReceiver(JSContext* cx, JS::HandleValue value, const JSClass* clasp,
                  JS::MutableHandleObject out) {
  out.set(value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  return (out && JS::GetClass(out) == clasp) || WebKit::Fail(cx, "Illegal track list receiver");
}
bool GetSlot(JSContext* cx, JS::HandleObject object, unsigned slot,
             JS::MutableHandleValue out) {
  out.set(JS::GetReservedSlot(object, slot));
  return JS_WrapValue(cx, out);
}
bool SetHandler(State& state, JS::HandleObject object, unsigned slot,
                const char* name, JS::HandleValue value) {
  if (!MediaHandlerSet(state, object, slot, name, value)) return false;
  return true;
}
bool StringEqual(JSContext* cx, JS::HandleString text, const char* ascii, bool* result) {
  return JS_StringEqualsAscii(cx, text, ascii, result);
}
bool Append(JSContext* cx, JS::HandleObject array, JS::HandleValue value) {
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, array, &length)) return false;
  return JS_SetElement(cx, array, length, value);
}
bool NumberSetter(JSContext* cx, JS::HandleValue value, double* out, const char* message) {
  if (!JS::ToNumber(cx, value, out)) return false;
  return std::isfinite(*out) || WebKit::FailKind(cx, message, JSEXN_RANGEERR);
}
}

bool TextTrackMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!TrackReceiver(cx, args.thisv(), &self)) return false;
  auto& state = *static_cast<State*>(JS::GetReservedSlot(self, TrackOwner).toPrivate());
  const int op = WebKit::Slot(args);
  if (op == TrackCues || op == TrackActiveCues) {
    // A disabled track exposes no cue lists (HTML TextTrack cues/activeCues).
    JS::RootedValue mode(cx, JS::GetReservedSlot(self, TrackMode));
    JS::RootedString text(cx, mode.toString());
    bool disabled = false;
    if (!StringEqual(cx, text, "disabled", &disabled)) return false;
    if (disabled) { args.rval().setNull(); return true; }
  }
  if (op < 100) return GetSlot(cx, self, op, args.rval());
  JS::RootedObject cue(cx);
  if (!argc || !CueReceiver(cx, args.get(0), &cue)) return false;
  JS::RootedValue listValue(cx, JS::GetReservedSlot(self, TrackCues));
  JS::RootedObject list(cx, listValue.isObject() ? &listValue.toObject() : nullptr);
  if (!list) return WebKit::Fail(cx, "TextTrack cues list is unavailable");
  JS::RootedValue itemsValue(cx, JS::GetReservedSlot(list, CueListItems));
  JS::RootedObject items(cx, itemsValue.isObject() ? &itemsValue.toObject() : nullptr);
  if (!items) return WebKit::Fail(cx, "TextTrack cue list is unavailable");
  if (op == 100) {
    JS::SetReservedSlot(cue, CueTrack, JS::ObjectValue(*self));
    if (!Append(cx, items, args[0])) return false;
  } else {
    uint32_t length = 0;
    if (!JS::GetArrayLength(cx, items, &length)) return false;
    bool found = false;
    for (uint32_t i = 0; i < length; ++i) {
      JS::RootedValue value(cx);
      if (!JS_GetElement(cx, items, i, &value)) return false;
      JS::RootedObject candidate(cx, value.isObject() ? &value.toObject() : nullptr);
      if (candidate && js::CheckedUnwrapStatic(candidate) == cue) {
        JS::ObjectOpResult removed;
        if (!JS_DeleteElement(cx, items, i, removed) || !removed.ok()) return false;
        found = true;
        break;
      }
    }
    if (!found) return MediaException(state, "NotFoundError", "TextTrack cue is not in the track");
    JS::SetReservedSlot(cue, CueTrack, JS::NullValue());
  }
  args.rval().setUndefined();
  return true;
}

bool TextTrackSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!TrackReceiver(cx, args.thisv(), &self)) return false;
  if (!argc) return WebKit::Fail(cx, "TextTrack setter requires an argument");
  auto& state = *static_cast<State*>(JS::GetReservedSlot(self, TrackOwner).toPrivate());
  const int slot = WebKit::Slot(args);
  if (slot == TrackMode) {
    JS::RootedString text(cx, JS::ToString(cx, args[0]));
    if (!text) return false;
    bool disabled = false, hidden = false, showing = false;
    if (!StringEqual(cx, text, "disabled", &disabled) ||
        !StringEqual(cx, text, "hidden", &hidden) ||
        !StringEqual(cx, text, "showing", &showing)) return false;
    // An enumeration attribute ignores a value outside TextTrackMode (WebIDL).
    if (disabled || hidden || showing) JS::SetReservedSlot(self, slot, JS::StringValue(text));
  } else {
    const char* names[] = {"cuechange"};
    if (!SetHandler(state, self, slot, names[slot - TrackCueChange], args[0])) return false;
  }
  args.rval().setUndefined();
  return true;
}

bool TextCueMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!CueReceiver(cx, args.thisv(), &self)) return false;
  return GetSlot(cx, self, WebKit::Slot(args), args.rval());
}

bool TextCueSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!CueReceiver(cx, args.thisv(), &self)) return false;
  if (!argc) return WebKit::Fail(cx, "TextTrackCue setter requires an argument");
  const int slot = WebKit::Slot(args);
  JS::RootedValue value(cx, args[0]);
  if (slot == CueId) {
    JS::RootedString text(cx, JS::ToString(cx, value));
    if (!text) return false;
    value.setString(text);
  } else if (slot == CueStart || slot == CueEnd) {
    double number = 0;
    if (!NumberSetter(cx, value, &number, "TextTrackCue time is not finite")) return false;
    value.setDouble(number);
  } else if (slot == CuePause) {
    value.setBoolean(JS::ToBoolean(value));
  } else {
    auto& state = *static_cast<State*>(JS::GetReservedSlot(self, CueOwner).toPrivate());
    const char* names[] = {"enter", "exit"};
    if (!SetHandler(state, self, slot, names[slot - CueEnter], value)) return false;
    args.rval().setUndefined();
    return true;
  }
  JS::SetReservedSlot(self, slot, value);
  args.rval().setUndefined();
  return true;
}

bool VTTCueMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!CueReceiver(cx, args.thisv(), &self) || JS::GetClass(self) != &kVTTCueClass) return false;
  const int op = WebKit::Slot(args);
  if (op == 100) return GetSlot(cx, self, VttText, args.rval());
  return GetSlot(cx, self, op, args.rval());
}

bool VTTCueSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!CueReceiver(cx, args.thisv(), &self) || JS::GetClass(self) != &kVTTCueClass) return false;
  if (!argc) return WebKit::Fail(cx, "VTTCue setter requires an argument");
  const int slot = WebKit::Slot(args);
  JS::RootedValue value(cx, args[0]);
  if (slot == VttRegion) {
    if (!value.isNull() && !(value.isObject() && IsVTTRegion(&value.toObject())))
      return WebKit::Fail(cx, "VTTCue.region setter: Value being assigned does not implement interface VTTRegion.");
  } else if (slot == VttVertical || slot == VttLineAlign || slot == VttPositionAlign || slot == VttAlign) {
    JS::RootedString text(cx, JS::ToString(cx, value));
    if (!text) return false;
    value.setString(text);
  } else if (slot == VttSnap) {
    value.setBoolean(JS::ToBoolean(value));
  } else if (slot == VttText) {
    JS::RootedString text(cx, JS::ToString(cx, value));
    if (!text) return false;
    value.setString(text);
  } else {
    double number = 0;
    if (!NumberSetter(cx, value, &number, "VTTCue numeric value is not finite")) return false;
    value.setDouble(number);
  }
  JS::SetReservedSlot(self, slot, value);
  args.rval().setUndefined();
  return true;
}
}
