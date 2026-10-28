// The two T3c interfaces whose dictionaries extend UIEvent/MouseEvent, plus the
// legacy initializer Firefox still exposes on each: DragEvent (MouseEvent
// subclass, initDragEvent) and CompositionEvent (UIEvent subclass,
// initCompositionEvent). Both reuse the parent field writers from
// event_init.cpp (InitUiFields/InitMouseFields), so the parent dictionary and
// the subclass dictionary each have exactly one adapter.
#include "interaction/internal.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include <string>

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
bool GetDragDataTransfer(JSContext* cx, unsigned argc, JS::Value* vp) {
  return Slot(cx, JS::CallArgsFromVp(argc, vp), "dataTransfer", Iface::Drag,
              kEventDragDataTransfer);
}
bool GetCompositionData(JSContext* cx, unsigned argc, JS::Value* vp) {
  return Slot(cx, JS::CallArgsFromVp(argc, vp), "data", Iface::Composition,
              kEventCompositionData);
}
// initDragEvent(16) (dom/webidl/DragEvent.webidl). The legacy mouse initializer
// rewrites the UIEvent and MouseEvent fields and the drag payload, and does not
// touch the movement fields (the donor's InitMouseEventInternal has none).
bool InitDragEvent(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::Drag);
  if (!event) return false;
  ReportEventAccess(cx, event, "DragEvent", "initDragEvent", int(ApiAccess::Call));
  if (args.length() < 1)
    return Error(cx, "TypeError", "DragEvent.initDragEvent requires a type");
  args.rval().setUndefined();
  if (event->value.dispatching()) return true;
  JS::RootedObject object(cx, Self(args));
  if (!LegacyBase(cx, event, args)) return false;
  int detail = 0, screen_x = 0, screen_y = 0, client_x = 0, client_y = 0, button = 0;
  if (!LegacyInt(cx, args, 4, &detail) || !LegacyInt(cx, args, 5, &screen_x) ||
      !LegacyInt(cx, args, 6, &screen_y) || !LegacyInt(cx, args, 7, &client_x) ||
      !LegacyInt(cx, args, 8, &client_y) || !LegacyInt(cx, args, 13, &button)) return false;
  bool ctrl = false, alt = false, shift = false, meta = false;
  LegacyBool(args, 9, &ctrl);
  LegacyBool(args, 10, &alt);
  LegacyBool(args, 11, &shift);
  LegacyBool(args, 12, &meta);
  JS::RootedValue view(cx), related(cx), transfer(cx);
  if (!LegacyObject(cx, args, 3, &view) || !LegacyObject(cx, args, 14, &related) ||
      !LegacyObject(cx, args, 15, &transfer)) return false;
  SetSlotInt(object, kEventUiDetail, detail);
  JS::SetReservedSlot(object, kEventView, view);
  SetSlotInt(object, kEventModifiers,
             static_cast<int32_t>((alt ? domain::kAltMask : 0) |
                                  (ctrl ? domain::kControlMask : 0) |
                                  (meta ? domain::kMetaMask : 0) |
                                  (shift ? domain::kShiftMask : 0)));
  SetSlotInt(object, kEventScreenX, screen_x);
  SetSlotInt(object, kEventScreenY, screen_y);
  SetSlotInt(object, kEventClientX, client_x);
  SetSlotInt(object, kEventClientY, client_y);
  SetSlotInt(object, kEventButton, button);
  JS::SetReservedSlot(object, kEventRelatedTarget, related);
  JS::SetReservedSlot(object, kEventDragDataTransfer, transfer);
  return true;
}

// initCompositionEvent(5) (dom/webidl/CompositionEvent.webidl): the donor
// resets the UIEvent fields, so detail returns to 0 while the data string is
// replaced.
bool InitCompositionEvent(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::Composition);
  if (!event) return false;
  ReportEventAccess(cx, event, "CompositionEvent", "initCompositionEvent",
                    int(ApiAccess::Call));
  if (args.length() < 1)
    return Error(cx, "TypeError", "CompositionEvent.initCompositionEvent requires a type");
  args.rval().setUndefined();
  if (event->value.dispatching()) return true;
  JS::RootedObject object(cx, Self(args));
  if (!LegacyBase(cx, event, args)) return false;
  JS::RootedValue view(cx);
  std::string data;
  if (!LegacyObject(cx, args, 3, &view) ||
      !LegacyString(cx, args, 4, &data)) return false;
  SetSlotInt(object, kEventUiDetail, 0);
  JS::SetReservedSlot(object, kEventView, view);
  return SetSlotText(cx, object, kEventCompositionData, data);
}
}  // namespace

// Event::InitEvent semantics shared by the four legacy initializers (DragEvent
// and CompositionEvent here, the device sensor pair in event_device.cpp;
// dom/events/Event.cpp:594-626). The caller has already applied the "no-op
// while dispatching" guard its donor applies, so this replaces
// type/bubbles/cancelable, clears the default-prevented and trusted flags and
// drops the dispatch target. The composed flag and the event's realm identity
// are untouched, exactly like the donor's InitEvent. The five helpers stay at
// namespace scope (internal.h) so the device initializers reuse them instead of
// defining a second copy of the legacy argument conversions.
bool LegacyBase(JSContext* cx, Event* event, const JS::CallArgs& args) {
  JS::RootedString text(cx);
  std::string key;
  if (!Text(cx, args.get(0), &text, &key)) return false;
  bool bubbles = false, cancelable = false;
  if (args.hasDefined(1)) bubbles = JS::ToBoolean(args[1]);
  if (args.hasDefined(2)) cancelable = JS::ToBoolean(args[2]);
  event->type.set(text.get());
  event->value.initialize(domain::EventType(std::move(key)), bubbles, cancelable);
  return true;
}
// One optional long argument of a legacy initializer; an absent or undefined
// argument keeps the spec default the caller pre-loaded.
bool LegacyInt(JSContext* cx, const JS::CallArgs& args, unsigned index, int* out) {
  if (!args.hasDefined(index)) return true;
  int32_t value = 0;
  if (!JS::ToInt32(cx, args[index], &value)) return false;
  *out = static_cast<int>(value);
  return true;
}
bool LegacyBool(const JS::CallArgs& args, unsigned index, bool* out) {
  if (args.hasDefined(index)) *out = JS::ToBoolean(args[index]);
  return true;
}
// One optional nullable-interface argument (Window?, DataTransfer?,
// EventTarget?): undefined and null stay null and a primitive is the WebIDL
// conversion TypeError. The interface brand itself is not checked because the
// referenced interfaces are owned by other domains.
bool LegacyObject(JSContext* cx, const JS::CallArgs& args, unsigned index,
                  JS::MutableHandleValue out) {
  out.setNull();
  if (!args.hasDefined(index) || args[index].isNullOrUndefined()) return true;
  if (!args[index].isObject())
    return Error(cx, "TypeError", "Event initializer requires an object");
  out.set(args[index]);
  return true;
}
// One optional DOMString? argument; null is an empty string, matching the
// donor's void-string conversion of a null dataArg.
bool LegacyString(JSContext* cx, const JS::CallArgs& args, unsigned index,
                  std::string* out) {
  if (!args.hasDefined(index) || args[index].isNullOrUndefined()) return true;
  JS::RootedString text(cx, JS::ToString(cx, args[index]));
  if (!text) return false;
  JS::UniqueChars encoded = JS_EncodeStringToUTF8(cx, text);
  if (!encoded) return false;
  out->assign(encoded.get());
  return true;
}

extern const JSPropertySpec kDragProperties[] = {
  JS_PSG("dataTransfer", GetDragDataTransfer, JSPROP_ENUMERATE),
  JS_PS_END
};
extern const JSFunctionSpec kDragMethods[] = {
  JS_FN("initDragEvent", InitDragEvent, 1, JSPROP_ENUMERATE),
  JS_FS_END
};
extern const JSPropertySpec kCompositionProperties[] = {
  JS_PSG("data", GetCompositionData, JSPROP_ENUMERATE),
  JS_PS_END
};
extern const JSFunctionSpec kCompositionMethods[] = {
  JS_FN("initCompositionEvent", InitCompositionEvent, 1, JSPROP_ENUMERATE),
  JS_FS_END
};

// DragEventInit extends MouseEventInit with a nullable DataTransfer; the
// referenced interface is owned by another domain, so the value is carried
// verbatim (null by default) and the dictionary conversion rule is enforced.
bool InitDrag(JSContext* cx, JS::HandleObject dict, JS::HandleObject object) {
  JS::RootedValue transfer(cx);
  if (!ReadNullableObject(cx, dict, "dataTransfer", &transfer)) return false;
  JS::SetReservedSlot(object, kEventDragDataTransfer, transfer);
  return true;
}

// CompositionEventInit adds one DOMString, default "".
bool InitComposition(JSContext* cx, JS::HandleObject dict, JS::HandleObject object) {
  std::string data;
  if (dict && !ReadInitString(cx, dict, "data", &data)) return false;
  return SetSlotText(cx, object, kEventCompositionData, data);
}
}  // namespace ruyijs::interaction
