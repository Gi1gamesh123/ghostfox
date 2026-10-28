// Firefox event interfaces with WebIDL-exact construction (docs/owners/INTERACTION.md,
// 2026-10-21): PopupBlockedEvent, CommandEvent, MediaStreamTrackEvent and
// MediaRecorderErrorEvent are constructible; TimeEvent, MouseScrollEvent and
// ScrollAreaEvent are Gecko-internal (no constructor, no createEvent alias).
// Init dictionary members are read in WebIDL order (EventInit first, then the
// derived members lexicographically) with Firefox's messages; the Element,
// MediaStreamTrack and DOMException brands come from their owners via the
// Interaction brand port. Values live in the event's public slots.
#include "interaction/internal.h"
#include "interaction/event_legacy_conversion.h"
#include "runtime/native_constructor_trace.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
#include <chrono>
#include <string>

namespace ruyijs::interaction {
bool IsFirefoxPayloadEvent(Iface iface) {
  return iface == Iface::PopupBlocked || iface == Iface::TimeEvent || iface == Iface::MouseScroll ||
         iface == Iface::ScrollArea || iface == Iface::Command || iface == Iface::MediaStreamTrack ||
         iface == Iface::MediaRecorderError || iface == Iface::RtcTransform;
}
namespace {
std::string Name(Iface iface) { return InterfaceName(iface); }
bool Fail(JSContext* cx, const std::string& message) { return Error(cx, "TypeError", message.c_str()); }
bool IsWindow(JS::HandleValue value) {
  JSObject* object = value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr;
  return object && JS_IsGlobalObject(object);
}
bool Brand(State& state, int brand, JS::HandleValue value) {
  bool matches = false;
  return state.brandPort && state.brandPort(state.brandContext, brand, value, &matches) && matches;
}
// One dictionary member: nullable interface types checked against their brand.
bool Member(JSContext* cx, JS::HandleObject dict, const char* name, JS::MutableHandleValue out) {
  out.setUndefined();
  return !dict || JS_GetProperty(cx, dict, name, out);
}
bool InterfaceMember(JSContext* cx, State& state, const std::string& iface, const char* init, const char* name,
                     const char* type, int brand, JS::HandleValue value) {
  const std::string prefix = iface + " constructor: '" + name + "' member of " + init;
  if (!value.isObject()) return Fail(cx, prefix + " is not an object.");
  const bool ok = brand < 0 ? IsWindow(value) : Brand(state, brand, value);
  return ok || Fail(cx, prefix + " does not implement interface " + type + ".");
}
bool StringMember(JSContext* cx, JS::HandleObject dict, const char* name, JS::MutableHandleValue out) {
  if (!Member(cx, dict, name, out)) return false;
  if (out.isUndefined()) { out.setString(JS_GetEmptyString(cx)); return true; }
  JSString* text = JS::ToString(cx, out);
  if (!text) return false;
  out.setString(text);
  return true;
}
// Derived members in lexicographic order, into public slots First...
bool ReadMembers(JSContext* cx, State& state, Iface iface, JS::HandleObject dict, JS::Value* values) {
  const std::string name = Name(iface);
  JS::RootedValue v(cx);
  switch (iface) {
    case Iface::PopupBlocked: {
      const char* init = "PopupBlockedEventInit";
      if (!StringMember(cx, dict, "popupWindowFeatures", &v)) return false;
      values[3] = v;
      if (!StringMember(cx, dict, "popupWindowName", &v)) return false;
      values[2] = v;
      if (!Member(cx, dict, "popupWindowURI", &v)) return false;
      if (!v.isNullOrUndefined()) {  // URI is an XPCOM interface content cannot supply
        if (!v.isObject()) return Fail(cx, name + " constructor: 'popupWindowURI' member of " + init + " is not an object.");
        return Fail(cx, name + " constructor: 'popupWindowURI' member of " + init + " does not implement interface URI.");
      }
      values[1] = JS::NullValue();
      if (!Member(cx, dict, "requestingWindow", &v)) return false;
      if (!v.isNullOrUndefined() && !InterfaceMember(cx, state, name, init, "requestingWindow", "Window", -1, v))
        return false;
      values[0] = v.isNullOrUndefined() ? JS::NullValue() : v.get();
      return true;
    }
    case Iface::Command: {
      if (!StringMember(cx, dict, "command", &v)) return false;
      values[0] = v;
      if (!Member(cx, dict, "source", &v)) return false;
      if (!v.isNullOrUndefined() &&
          !InterfaceMember(cx, state, name, "CommandEventInit", "source", "Element", kBrandElement, v)) return false;
      values[1] = v.isNullOrUndefined() ? JS::NullValue() : v.get();
      return true;
    }
    default: {  // MediaStreamTrackEvent.track / MediaRecorderErrorEvent.error / RTCTransformEvent.transformer: required
      if (iface == Iface::RtcTransform) {
        if (!Member(cx, dict, "transformer", &v)) return false;
        if (v.isUndefined())
          return Fail(cx, name + " constructor: Missing required 'transformer' member of RTCTransformEventInit.");
        if (!InterfaceMember(cx, state, name, "RTCTransformEventInit", "transformer",
                             "RTCRtpScriptTransformer", kBrandRtcScriptTransformer, v)) return false;
        values[0] = v;
        return true;
      }
      const bool track = iface == Iface::MediaStreamTrack;
      const char* member = track ? "track" : "error";
      const char* init = track ? "MediaStreamTrackEventInit" : "MediaRecorderErrorEventInit";
      if (!Member(cx, dict, member, &v)) return false;
      if (v.isUndefined())
        return Fail(cx, name + " constructor: Missing required '" + member + "' member of " + init + ".");
      if (!InterfaceMember(cx, state, name, init, member, track ? "MediaStreamTrack" : "DOMException",
                           track ? kBrandMediaStreamTrack : kBrandDomException, v)) return false;
      values[0] = v;
      return true;
    }
  }
}
bool Construct(JSContext* cx, const JS::CallArgs& args, Iface iface, Kind kind, unsigned arity) {
  NativeConstructorTrace trace(cx, args);
  const std::string name = Name(iface);
  if (!args.isConstructing()) return Fail(cx, name + " constructor: 'new' is required");
  if (args.length() < arity)
    return Fail(cx, name + " constructor: At least " + std::to_string(arity) + " argument" + (arity == 1 ? "" : "s") +
                    " required, but only " + std::to_string(args.length()) + " passed");
  State* state = activeState;
  while (state && state->page != JS::CurrentGlobalOrNull(cx)) state = state->previous;
  if (!state) return Error(cx, "TypeError", "Event realm is not installed");
  state->Observe(name.c_str(), "constructor", int(ApiAccess::Construct));
  if (state->events.size() >= 100000) return Error(cx, "RangeError", "Event limit exceeded");
  JS::RootedString type(cx);
  std::string key;
  if (!Text(cx, args[0], &type, &key)) return false;
  JS::RootedValue init(cx, args.get(1));
  if (!init.isNullOrUndefined() && !init.isObject())
    return Fail(cx, name + " constructor: Value can't be converted to a dictionary.");
  JS::RootedObject dict(cx, init.isObject() ? &init.toObject() : nullptr);
  bool bubbles = false, cancelable = false, composed = false;
  if (dict && (!Option(cx, dict, "bubbles", &bubbles) || !Option(cx, dict, "cancelable", &cancelable) ||
               !Option(cx, dict, "composed", &composed))) return false;
  JS::RootedValueArray<4> values(cx);
  if (!ReadMembers(cx, *state, iface, dict, values.begin())) return false;
  JS::RootedObject object(cx, JS_NewObjectForConstructor(cx, &kEventClass, args));
  if (!object) return false;
  JS::SetReservedSlot(object, kEventKind, JS::Int32Value(static_cast<int32_t>(kind)));
  if (!InstallTrusted(*state, object)) return false;
  for (uint32_t i = 0; i < 4; ++i) JS::SetReservedSlot(object, kEventPublicFirst + i, values[i]);
  auto event = std::make_unique<Event>(cx, object, type, std::move(key), bubbles, cancelable);
  event->kind = kind;
  event->value.set_composed(composed);
  event->value.set_time_stamp(std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - state->started).count());
  state->events.push_back(std::move(event));
  args.rval().setObject(*object);
  return trace.Finish(true);
}
}  // namespace

#define PAYLOAD_CONSTRUCTOR(function, iface, arity)                                              \
  bool function(JSContext* cx, unsigned argc, JS::Value* vp) {                                   \
    return Construct(cx, JS::CallArgsFromVp(argc, vp), Iface::iface, Kind::iface, arity);      \
  }
PAYLOAD_CONSTRUCTOR(PopupBlockedConstructor, PopupBlocked, 1)
PAYLOAD_CONSTRUCTOR(CommandConstructor, Command, 1)
PAYLOAD_CONSTRUCTOR(MediaStreamTrackConstructor, MediaStreamTrack, 2)
PAYLOAD_CONSTRUCTOR(MediaRecorderErrorConstructor, MediaRecorderError, 2)
PAYLOAD_CONSTRUCTOR(RTCTransformConstructor, RtcTransform, 2)
#undef PAYLOAD_CONSTRUCTOR
}  // namespace ruyijs::interaction
