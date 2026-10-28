#include "web_media/text_track.h"
#include "web_media/media_runtime.h"
#include "web_media/state.h"
#include "runtime/native_constructor_trace.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include <cmath>

namespace ruyijs::web_media {
const JSClass kTextTrackClass = {"TextTrack", JSCLASS_HAS_RESERVED_SLOTS(TrackSlotCount)};
const JSClass kTextTrackCueClass = {"TextTrackCue", JSCLASS_HAS_RESERVED_SLOTS(CueSlotCount)};
const JSClass kTextTrackCueListClass = {"TextTrackCueList", JSCLASS_HAS_RESERVED_SLOTS(CueListSlotCount)};
const JSClass kTextTrackListClass = {"TextTrackList", JSCLASS_HAS_RESERVED_SLOTS(TrackListSlotCount)};
const JSClass kVTTCueClass = {"VTTCue", JSCLASS_HAS_RESERVED_SLOTS(VttCueSlotCount)};

namespace {
JSString* Text(JSContext* cx, const char* value) {
  return JS_NewStringCopyZ(cx, value);
}
bool Exact(JSObject* object, const JSClass* clasp) {
  return object && JS::GetClass(object) == clasp;
}
bool CueObject(JSContext* cx, JS::HandleValue value, JS::MutableHandleObject out) {
  out.set(value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  if (Exact(out, &kTextTrackCueClass) || Exact(out, &kVTTCueClass)) return true;
  return WebKit::Fail(cx, "Illegal TextTrackCue receiver");
}
bool StringSlot(JSContext* cx, JS::HandleObject object, unsigned slot,
                JS::MutableHandleValue out) {
  out.set(JS::GetReservedSlot(object, slot));
  return JS_WrapValue(cx, out);
}
bool MakeList(State& state, const JSClass* clasp, unsigned slotCount,
              JS::MutableHandleObject out, bool events) {
  JS::RootedObject proto(state.cx, clasp == &kTextTrackCueListClass
      ? state.cueList.prototype : state.trackList.prototype);
  JS::RootedObject object(state.cx, state.kit.New(clasp, proto));
  JS::RootedObject items(state.cx, JS::NewArrayObject(state.cx, 0));
  if (!object || !items) return false;
  if (events && !state.events.Attach(object)) return false;
  JS::SetReservedSlot(object, 0, JS::ObjectValue(*items));
  for (unsigned i = 1; i < slotCount; ++i) JS::SetReservedSlot(object, i, JS::NullValue());
  if (clasp == &kTextTrackCueListClass)
    JS::SetReservedSlot(object, CueListOwner, JS::PrivateValue(&state));
  else
    JS::SetReservedSlot(object, TrackListOwner, JS::PrivateValue(&state));
  out.set(object);
  return true;
}
bool InitCue(State& state, JS::HandleObject object) {
  JS::RootedString empty(state.cx, Text(state.cx, ""));
  if (!empty || !state.events.Attach(object)) return false;
  JS::SetReservedSlot(object, CueTrack, JS::NullValue());
  JS::SetReservedSlot(object, CueId, JS::StringValue(empty));
  JS::SetReservedSlot(object, CueStart, JS::DoubleValue(0));
  JS::SetReservedSlot(object, CueEnd, JS::DoubleValue(0));
  JS::SetReservedSlot(object, CuePause, JS::BooleanValue(false));
  JS::SetReservedSlot(object, CueEnter, JS::NullValue());
  JS::SetReservedSlot(object, CueExit, JS::NullValue());
  JS::SetReservedSlot(object, CueOwner, JS::PrivateValue(&state));
  return true;
}
bool InitVtt(State& state, JS::HandleObject object, JS::HandleString text) {
  JS::RootedString empty(state.cx, Text(state.cx, ""));
  JS::RootedString start(state.cx, Text(state.cx, "start"));
  JS::RootedString automatic(state.cx, Text(state.cx, "auto"));
  JS::RootedString center(state.cx, Text(state.cx, "center"));
  if (!empty || !start || !automatic || !center || !InitCue(state, object)) return false;
  JS::SetReservedSlot(object, VttRegion, JS::NullValue());
  JS::SetReservedSlot(object, VttVertical, JS::StringValue(empty));
  JS::SetReservedSlot(object, VttSnap, JS::BooleanValue(true));
  JS::SetReservedSlot(object, VttLine, JS::DoubleValue(-1));
  JS::SetReservedSlot(object, VttLineAlign, JS::StringValue(start));
  JS::SetReservedSlot(object, VttPosition, JS::DoubleValue(50));
  JS::SetReservedSlot(object, VttPositionAlign, JS::StringValue(automatic));
  JS::SetReservedSlot(object, VttSize, JS::DoubleValue(100));
  JS::SetReservedSlot(object, VttAlign, JS::StringValue(center));
  JS::SetReservedSlot(object, VttText, JS::StringValue(text));
  JS::SetReservedSlot(object, VttOwner, JS::PrivateValue(&state));
  return true;
}
}

JSObject* NewTextTrack(State& state, const char* kind, const char* label, const char* language) {
  JS::RootedObject object(state.cx, state.kit.New(&kTextTrackClass, state.textTrack.prototype));
  JS::RootedObject cues(state.cx), active(state.cx);
  JS::RootedString k(state.cx, Text(state.cx, kind)), l(state.cx, Text(state.cx, label)),
      lang(state.cx, Text(state.cx, language)), id(state.cx, Text(state.cx, "")),
      dispatch(state.cx, Text(state.cx, "")), mode(state.cx, Text(state.cx, "disabled"));
  if (!object || !k || !l || !lang || !id || !dispatch || !mode ||
      !MakeList(state, &kTextTrackCueListClass, CueListSlotCount, &cues, false) ||
      !MakeList(state, &kTextTrackCueListClass, CueListSlotCount, &active, false) ||
      !state.events.Attach(object)) return nullptr;
  JS::SetReservedSlot(object, TrackKind, JS::StringValue(k));
  JS::SetReservedSlot(object, TrackLabel, JS::StringValue(l));
  JS::SetReservedSlot(object, TrackLanguage, JS::StringValue(lang));
  JS::SetReservedSlot(object, TrackId, JS::StringValue(id));
  JS::SetReservedSlot(object, TrackCues, JS::ObjectValue(*cues));
  JS::SetReservedSlot(object, TrackActiveCues, JS::ObjectValue(*active));
  JS::SetReservedSlot(object, TrackDispatchType, JS::StringValue(dispatch));
  JS::SetReservedSlot(object, TrackMode, JS::StringValue(mode));
  JS::SetReservedSlot(object, TrackCueChange, JS::NullValue());
  JS::SetReservedSlot(object, TrackOwner, JS::PrivateValue(&state));
  return object.get();
}

JSObject* NewTextTrackList(State& state) {
  JS::RootedObject object(state.cx);
  return MakeList(state, &kTextTrackListClass, TrackListSlotCount, &object, true) ? object.get() : nullptr;
}

bool TextTrackConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return trace.Finish(WebKit::Fail(cx, "Illegal constructor."));
}

bool TextCueConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return trace.Finish(WebKit::Fail(cx, "Illegal constructor."));
}

bool VTTCueConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  if (!args.isConstructing()) return trace.Finish(WebKit::Fail(cx, "VTTCue requires new"));
  if (argc < 3) return trace.Finish(WebKit::Fail(cx, "VTTCue requires 3 arguments"));
  double start = 0, end = 0;
  if (!JS::ToNumber(cx, args[0], &start) || !JS::ToNumber(cx, args[1], &end)) return false;
  if (!std::isfinite(start) || !std::isfinite(end) || start < 0 || end < start)
    return trace.Finish(WebKit::FailKind(cx, "VTTCue times are invalid", JSEXN_RANGEERR));
  JS::RootedString text(cx, JS::ToString(cx, args[2]));
  if (!text) return false;
  auto& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject proto(cx);
  if (!state.kit.ConstructProto(args, state.vttCue.prototype, &proto)) return false;
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kVTTCueClass, proto));
  if (!object || !InitVtt(state, object, text)) return false;
  JS::SetReservedSlot(object, CueStart, JS::DoubleValue(start));
  JS::SetReservedSlot(object, CueEnd, JS::DoubleValue(end));
  args.rval().setObject(*object);
  return trace.Finish(true);
}
}
