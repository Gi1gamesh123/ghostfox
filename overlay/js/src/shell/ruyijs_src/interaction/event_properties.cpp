#include "interaction/internal.h"
#include "js/PropertyAndElement.h"
#include "js/Conversions.h"

namespace ruyijs::interaction {
enum Property { Type, TargetValue, CurrentTarget, Phase, Bubbles, Cancelable, Composed,
                DefaultPrevented, Trusted, Timestamp, CancelBubble, ReturnValue };
// Canonical member name reported to the debug monitor. The target aliases
// (srcElement, originalTarget, explicitOriginalTarget) report the primary
// name, so one watch entry covers the field rather than each spelling.
static const char* PropertyName(Property property) {
  switch (property) {
    case Type: return "type";
    case TargetValue: return "target";
    case CurrentTarget: return "currentTarget";
    case Phase: return "eventPhase";
    case Bubbles: return "bubbles";
    case Cancelable: return "cancelable";
    case Composed: return "composed";
    case DefaultPrevented: return "defaultPrevented";
    case Trusted: return "isTrusted";
    case Timestamp: return "timeStamp";
    case CancelBubble: return "cancelBubble";
    case ReturnValue: return "returnValue";
  }
  return "";
}
static bool Get(JSContext* cx, unsigned argc, JS::Value* vp, Property property,
                bool cross_realm = false) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (property == Trusted && ReadPublicWindowTrusted(cx, args, cross_realm)) return true;
  Event* event = Receiver(cx, args, Iface::Event, cross_realm);
  if (!event) return false;
  auto& value = event->value;
  State* state = Owner(cx, event->object, true);
  // Observation only: the receiver is validated above, so an invalid receiver
  // still throws from the same place and records nothing.
  state->Observe("Event", PropertyName(property), int(ApiAccess::Read));
  switch (property) {
    case Type: args.rval().setString(event->type); break;
    case TargetValue: args.rval().setObjectOrNull(state->targetObject(value.target_id())); break;
    case CurrentTarget: args.rval().setObjectOrNull(state->targetObject(value.current_target_id())); break;
    case Phase: args.rval().setInt32(static_cast<int>(value.event_phase())); break;
    case Bubbles: args.rval().setBoolean(value.bubbles()); break;
    case Cancelable: args.rval().setBoolean(value.cancelable()); break;
    case Composed: args.rval().setBoolean(value.composed()); break;
    case DefaultPrevented: args.rval().setBoolean(value.default_prevented()); break;
    case Trusted: args.rval().setBoolean(value.is_trusted()); break;
    case Timestamp: args.rval().setDouble(value.time_stamp()); break;
    case CancelBubble: args.rval().setBoolean(value.propagation_stopped()); break;
    case ReturnValue: args.rval().setBoolean(!value.default_prevented()); break;
  }
  return JS_WrapValue(cx, args.rval());
}
#define GETTER(name, key) static bool name(JSContext* cx, unsigned argc, JS::Value* vp) { return Get(cx, argc, vp, key); }
GETTER(GetType, Type)
GETTER(GetTarget, TargetValue)
GETTER(GetCurrent, CurrentTarget)
GETTER(GetPhase, Phase)
GETTER(GetBubbles, Bubbles)
GETTER(GetCancelable, Cancelable)
GETTER(GetComposed, Composed)
GETTER(GetPrevented, DefaultPrevented)
GETTER(GetTimestamp, Timestamp)
GETTER(GetCancelBubble, CancelBubble)
GETTER(GetReturnValue, ReturnValue)
bool GetTrusted(JSContext* cx, unsigned argc, JS::Value* vp) { return Get(cx, argc, vp, Trusted); }
bool GetTrustedAcrossRealms(JSContext* cx, unsigned argc, JS::Value* vp) {
  return Get(cx, argc, vp, Trusted, true);
}
#undef GETTER
static bool SetCancelBubble(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args);
  if (!event) return false;
  ReportEventAccess(cx, event, "Event", "cancelBubble", int(ApiAccess::Write));
  if (JS::ToBoolean(args.get(0))) event->value.stop_propagation();
  args.rval().setUndefined(); return true;
}
static bool SetReturnValue(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args);
  if (!event) return false;
  ReportEventAccess(cx, event, "Event", "returnValue", int(ApiAccess::Write));
  if (!JS::ToBoolean(args.get(0))) event->value.prevent_default();
  args.rval().setUndefined(); return true;
}
extern const JSPropertySpec kEventProperties[] = {
  JS_PSG("type", GetType, JSPROP_ENUMERATE),
  JS_PSG("target", GetTarget, JSPROP_ENUMERATE),
  JS_PSG("srcElement", GetTarget, JSPROP_ENUMERATE),
  JS_PSG("originalTarget", GetTarget, JSPROP_ENUMERATE),
  JS_PSG("explicitOriginalTarget", GetTarget, JSPROP_ENUMERATE),
  JS_PSG("currentTarget", GetCurrent, JSPROP_ENUMERATE),
  JS_PSG("eventPhase", GetPhase, JSPROP_ENUMERATE),
  JS_PSG("bubbles", GetBubbles, JSPROP_ENUMERATE),
  JS_PSG("cancelable", GetCancelable, JSPROP_ENUMERATE),
  JS_PSG("composed", GetComposed, JSPROP_ENUMERATE),
  JS_PSG("defaultPrevented", GetPrevented, JSPROP_ENUMERATE),
  JS_PSG("timeStamp", GetTimestamp, JSPROP_ENUMERATE),
  JS_PSGS("cancelBubble", GetCancelBubble, SetCancelBubble, JSPROP_ENUMERATE),
  JS_PSGS("returnValue", GetReturnValue, SetReturnValue, JSPROP_ENUMERATE),
  JS_PS_END
};
}
