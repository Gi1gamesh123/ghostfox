#include "interaction/event_legacy_conversion.h"
#include "js/Object.h"

namespace ruyijs::interaction {
namespace {
bool InitUiEvent(JSContext* cx, unsigned argc, JS::Value* vp) {
  const auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::UI);
  if (!event) return false;
  ReportEventAccess(cx, event, "UIEvent", "initUIEvent", int(ApiAccess::Call));
  if (!argc) return Error(cx, "TypeError", "initUIEvent requires a type");
  args.rval().setUndefined();
  LegacyConversion conversion(cx);
  int32_t detail;
  if (!conversion.Type(args[0]) || !conversion.Int(args.get(4), &detail)) return false;
  conversion.Commit(event, args);
  SetSlotInt(event->object, kEventUiDetail, detail);
  JS::SetReservedSlot(event->object, kEventView, LegacyValue(args, 3));
  return conversion.Finish();
}
}  // namespace
bool InitLegacyMouseEventFields(JSContext* cx, Event* event,
                                const JS::CallArgs& args, bool extended) {
  if (!args.length()) {
    return extended ? Error(cx, "TypeError", "initNSMouseEvent requires a type") : true;
  }
  LegacyConversion conversion(cx);
  int32_t detail, screen_x, screen_y, client_x, client_y, button;
  uint16_t input_source = 0;
  float pressure = 0.0f;
  if (!conversion.Type(args[0]) || !conversion.Int(args.get(4), &detail) ||
      !conversion.Int(args.get(5), &screen_x) || !conversion.Int(args.get(6), &screen_y) ||
      !conversion.Int(args.get(7), &client_x) || !conversion.Int(args.get(8), &client_y) ||
      !conversion.Int(args.get(13), &button) ||
      (extended && args.hasDefined(15) && !conversion.Float(args[15], &pressure)) ||
      (extended && args.hasDefined(16) && !conversion.Uint16(args[16], &input_source)))
    return false;
  // Gecko's extended initializer is a no-op during dispatch. Keep the shared
  // ordinary initMouseEvent behavior unchanged for existing callers.
  if (extended && event->value.dispatching()) return conversion.Finish();
  conversion.Commit(event, args);
  JS::RootedObject object(cx, event->object);
  SetSlotInt(object, kEventUiDetail, detail);
  JS::SetReservedSlot(object, kEventView, LegacyValue(args, 3));
  SetSlotInt(object, kEventScreenX, screen_x);
  SetSlotInt(object, kEventScreenY, screen_y);
  SetSlotInt(object, kEventClientX, client_x);
  SetSlotInt(object, kEventClientY, client_y);
  const uint32_t short_bits = static_cast<uint32_t>(button) & 0xffff;
  SetSlotInt(object, kEventButton, short_bits < 0x8000 ? int32_t(short_bits)
                                                    : int32_t(short_bits) - 0x10000);
  SetSlotInt(object, kEventModifiers, LegacyModifiers(args, 9));
  JS::SetReservedSlot(object, kEventRelatedTarget, LegacyTarget(cx, args.get(14)));
  if (extended) {
    SetSlotNumber(object, kEventPressure, pressure);
    SetSlotInt(object, kEventInputSource, static_cast<int32_t>(input_source));
  }
  return conversion.Finish();
}
namespace {
bool InitMouseEventImpl(JSContext* cx, unsigned argc, JS::Value* vp, bool extended) {
  const auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::Mouse);
  if (!event) return false;
  const char* member = extended ? "initNSMouseEvent" : "initMouseEvent";
  ReportEventAccess(cx, event, "MouseEvent", member, int(ApiAccess::Call));
  args.rval().setUndefined();
  return InitLegacyMouseEventFields(cx, event, args, extended);
}
}
bool InitMouseEvent(JSContext* cx, unsigned argc, JS::Value* vp) {
  return InitMouseEventImpl(cx, argc, vp, false);
}
bool InitNSMouseEvent(JSContext* cx, unsigned argc, JS::Value* vp) {
  return InitMouseEventImpl(cx, argc, vp, true);
}
extern const JSFunctionSpec kUiMethods[] = {
  JS_FN("initUIEvent", InitUiEvent, 1, JSPROP_ENUMERATE), JS_FS_END};
}
