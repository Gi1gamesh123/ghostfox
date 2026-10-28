// Members of the Firefox event interfaces built in event_firefox_payloads.cpp (docs/owners/INTERACTION.md,
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
namespace {
std::string Name(Iface iface) { return InterfaceName(iface); }
bool Fail(JSContext* cx, const std::string& message) { return Error(cx, "TypeError", message.c_str()); }
bool IsWindow(JS::HandleValue value) {
  JSObject* object = value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr;
  return object && JS_IsGlobalObject(object);
}
// Brand-checked member access with Firefox's receiver message.
Event* Self(JSContext* cx, const JS::CallArgs& args, Iface iface, const char* member) {
  JS::RootedObject object(cx, args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  State* state = object && JS::GetClass(object) == &kEventClass ? Owner(cx, object, true) : nullptr;
  if (JS_IsExceptionPending(cx)) JS_ClearPendingException(cx);
  Event* event = state ? state->event(object) : nullptr;
  if (event && IsA(event->kind, iface)) {
    ReportEventAccess(cx, event, InterfaceName(iface), member, int(ApiAccess::Read));
    return event;
  }
  Fail(cx, std::string("'") + member + "' called on an object that does not implement interface " + Name(iface) + ".");
  return nullptr;
}
bool Slot(JSContext* cx, const JS::CallArgs& args, Iface iface, const char* member, uint32_t slot) {
  Event* event = Self(cx, args, iface, member);
  if (!event) return false;
  args.rval().set(JS::GetReservedSlot(event->object, slot));
  if (args.rval().isUndefined()) args.rval().setNull();
  return JS_WrapValue(cx, args.rval());
}
#define PAYLOAD_GETTER(function, iface, member, slot)                        \
  bool function(JSContext* cx, unsigned n, JS::Value* vp) {                   \
    return Slot(cx, JS::CallArgsFromVp(n, vp), Iface::iface, member, slot); \
  }
PAYLOAD_GETTER(RequestingWindow, PopupBlocked, "get requestingWindow", kEventPublicFirst)
PAYLOAD_GETTER(PopupUri, PopupBlocked, "get popupWindowURI", kEventPublicSecond)
PAYLOAD_GETTER(PopupName, PopupBlocked, "get popupWindowName", kEventPublicThird)
PAYLOAD_GETTER(PopupFeatures, PopupBlocked, "get popupWindowFeatures", kEventPublicFourth)
PAYLOAD_GETTER(CommandName, Command, "get command", kEventPublicFirst)
PAYLOAD_GETTER(CommandSource, Command, "get source", kEventPublicSecond)
PAYLOAD_GETTER(Track, MediaStreamTrack, "get track", kEventPublicFirst)
PAYLOAD_GETTER(RecorderError, MediaRecorderError, "get error", kEventPublicFirst)
PAYLOAD_GETTER(TimeView, TimeEvent, "get view", kEventPublicSecond)
#undef PAYLOAD_GETTER
bool Number(JSContext* cx, const JS::CallArgs& args, Iface iface, const char* member, uint32_t slot) {
  Event* event = Self(cx, args, iface, member);
  if (!event) return false;
  const JS::Value& value = JS::GetReservedSlot(event->object, slot);
  args.rval().setNumber(value.isNumber() ? value.toNumber() : 0.0);
  return true;
}
bool TimeDetail(JSContext* cx, unsigned n, JS::Value* vp) {
  return Number(cx, JS::CallArgsFromVp(n, vp), Iface::TimeEvent, "get detail", kEventPublicFirst);
}
bool Axis(JSContext* cx, unsigned n, JS::Value* vp) {
  return Number(cx, JS::CallArgsFromVp(n, vp), Iface::MouseScroll, "get axis", kEventPublicFirst);
}
#define AREA_GETTER(function, member, index)                                                 \
  bool function(JSContext* cx, unsigned n, JS::Value* vp) {                                  \
    return Number(cx, JS::CallArgsFromVp(n, vp), Iface::ScrollArea, member, kEventPublicFirst + index); \
  }
AREA_GETTER(AreaX, "get x", 0)
AREA_GETTER(AreaY, "get y", 1)
AREA_GETTER(AreaWidth, "get width", 2)
AREA_GETTER(AreaHeight, "get height", 3)
#undef AREA_GETTER
// initTimeEvent(type, optional Window? view = null, optional long detail = 0)
bool InitTimeEvent(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Self(cx, args, Iface::TimeEvent, "initTimeEvent");
  if (!event) return false;
  if (!argc) return Fail(cx, "TimeEvent.initTimeEvent: At least 1 argument required, but only 0 passed");
  LegacyConversion conversion(cx);
  int32_t detail = 0;
  if (!conversion.Type(args[0])) return false;
  if (!args.get(1).isNullOrUndefined() && !IsWindow(args[1]))
    return Fail(cx, args[1].isObject() ? "TimeEvent.initTimeEvent: Argument 2 does not implement interface Window."
                                       : "TimeEvent.initTimeEvent: Argument 2 is not an object.");
  if (!conversion.Int(args.get(2), &detail)) return false;
  args.rval().setUndefined();
  if (event->value.dispatching()) return conversion.Finish();
  conversion.Commit(event, args);
  JS::SetReservedSlot(event->object, kEventPublicFirst, JS::Int32Value(detail));
  JS::SetReservedSlot(event->object, kEventPublicSecond, args.get(1).isObject() ? args[1] : JS::NullValue());
  return conversion.Finish();
}
// initMouseScrollEvent(type, canBubble, cancelable, view, detail, screenX, screenY,
// clientX, clientY, ctrl, alt, shift, meta, button, relatedTarget, axis)
bool InitMouseScrollEvent(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Self(cx, args, Iface::MouseScroll, "initMouseScrollEvent");
  if (!event) return false;
  if (!argc) return Fail(cx, "MouseScrollEvent.initMouseScrollEvent: At least 1 argument required, but only 0 passed");
  int32_t axis = 0;
  if (!JS::ToInt32(cx, args.get(15), &axis)) return false;
  args.rval().setUndefined();
  JS::RootedObject object(cx, event->object);
  if (!InitLegacyMouseEventFields(cx, event, args, false)) return false;
  JS::SetReservedSlot(object, kEventPublicFirst, JS::Int32Value(axis));
  return true;
}
// initScrollAreaEvent(type, canBubble, cancelable, view, detail, x, y, width, height)
bool InitScrollAreaEvent(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Self(cx, args, Iface::ScrollArea, "initScrollAreaEvent");
  if (!event) return false;
  if (!argc) return Fail(cx, "ScrollAreaEvent.initScrollAreaEvent: At least 1 argument required, but only 0 passed");
  LegacyConversion conversion(cx);
  int32_t detail = 0;
  float area[4] = {0, 0, 0, 0};
  if (!conversion.Type(args[0]) || !conversion.Int(args.get(4), &detail)) return false;
  for (unsigned i = 0; i < 4; ++i)
    if (!conversion.Float(args.get(5 + i), &area[i])) return false;
  args.rval().setUndefined();
  if (event->value.dispatching()) return conversion.Finish();
  conversion.Commit(event, args);
  SetSlotInt(event->object, kEventUiDetail, detail);
  JS::SetReservedSlot(event->object, kEventView, LegacyValue(args, 3));
  for (unsigned i = 0; i < 4; ++i)
    JS::SetReservedSlot(event->object, kEventPublicFirst + i, JS::DoubleValue(area[i]));
  return conversion.Finish();
}
}  // namespace


extern const JSPropertySpec kPopupBlockedProperties[] = {
  JS_PSG("requestingWindow", RequestingWindow, JSPROP_ENUMERATE),
  JS_PSG("popupWindowURI", PopupUri, JSPROP_ENUMERATE),
  JS_PSG("popupWindowName", PopupName, JSPROP_ENUMERATE),
  JS_PSG("popupWindowFeatures", PopupFeatures, JSPROP_ENUMERATE), JS_PS_END};
extern const JSPropertySpec kCommandProperties[] = {
  JS_PSG("command", CommandName, JSPROP_ENUMERATE), JS_PSG("source", CommandSource, JSPROP_ENUMERATE), JS_PS_END};
extern const JSPropertySpec kMediaStreamTrackProperties[] = {JS_PSG("track", Track, JSPROP_ENUMERATE), JS_PS_END};
extern const JSPropertySpec kMediaRecorderErrorProperties[] = {
  JS_PSG("error", RecorderError, JSPROP_ENUMERATE), JS_PS_END};
extern const JSPropertySpec kTimeEventProperties[] = {
  JS_PSG("detail", TimeDetail, JSPROP_ENUMERATE), JS_PSG("view", TimeView, JSPROP_ENUMERATE), JS_PS_END};
extern const JSFunctionSpec kTimeEventMethods[] = {
  JS_FN("initTimeEvent", InitTimeEvent, 1, JSPROP_ENUMERATE), JS_FS_END};
extern const JSPropertySpec kMouseScrollProperties[] = {JS_PSG("axis", Axis, JSPROP_ENUMERATE), JS_PS_END};
extern const JSFunctionSpec kMouseScrollMethods[] = {
  JS_FN("initMouseScrollEvent", InitMouseScrollEvent, 1, JSPROP_ENUMERATE), JS_FS_END};
extern const EventConstant kMouseScrollConstants[] = {{"HORIZONTAL_AXIS", 1}, {"VERTICAL_AXIS", 2}, {nullptr, 0}};
extern const JSPropertySpec kScrollAreaProperties[] = {
  JS_PSG("x", AreaX, JSPROP_ENUMERATE), JS_PSG("y", AreaY, JSPROP_ENUMERATE),
  JS_PSG("width", AreaWidth, JSPROP_ENUMERATE), JS_PSG("height", AreaHeight, JSPROP_ENUMERATE), JS_PS_END};
extern const JSFunctionSpec kScrollAreaMethods[] = {
  JS_FN("initScrollAreaEvent", InitScrollAreaEvent, 1, JSPROP_ENUMERATE), JS_FS_END};
}  // namespace ruyijs::interaction
