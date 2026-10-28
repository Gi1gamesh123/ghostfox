#include "interaction/touch.h"
#include "interaction/event_keyboard.h"
#include "interaction/event_legacy_conversion.h"
#include "js/Object.h"
#include "js/Realm.h"

namespace ruyijs::interaction {
bool InitTouchEvent(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::Touch, true);
  if (!event) return false;
  ReportEventAccess(cx, event, "TouchEvent", "initTouchEvent", int(ApiAccess::Call));
  if (!argc) return Error(cx, "TypeError", "initTouchEvent requires a type");
  LegacyConversion conversion(cx);
  JS::RootedValue view(cx);
  int32_t detail;
  // Unlike older public event adapters, any conversion exception leaves this
  // event entirely unchanged. Finish before the next argument and before commit.
  if (!conversion.Type(args[0]) || !conversion.Finish() ||
      !KeyboardView(cx, args.get(3), &view) ||
      !conversion.Int(args.get(4), &detail) || !conversion.Finish()) return false;
  const uint32_t modifiers = LegacyModifiers(args, 5);
  JS::RootedValueArray<3> lists(cx);
  for (unsigned i = 0; i < 3; ++i) {
    lists[i].set(args.get(9 + i));
    if (lists[i].isNullOrUndefined()) lists[i].setNull();
    else if (!TouchListReceiver(cx, lists[i])) return false;
  }
  args.rval().setUndefined();
  if (event->value.dispatching()) return true;
  State* owner = Owner(cx, event->object, true);
  if (!owner) return Error(cx, "TypeError", "TouchEvent owner is not installed");
  JSAutoRealm enter(cx, owner->page);
  JS::RootedObject members(cx);
  for (unsigned i = 0; i < 3; ++i) if (!JS_WrapValue(cx, lists[i])) return false;
  if (!JS_WrapValue(cx, &view) || !BuildTouchMembers(*owner, lists, &members)) return false;
  conversion.Commit(event, args);
  const unsigned slots[] = {kEventTouches, kEventTargetTouches, kEventChangedTouches};
  for (unsigned i = 0; i < 3; ++i) JS::SetReservedSlot(event->object, slots[i], lists[i]);
  JS::SetReservedSlot(event->object, kTouchWidgetMembers, JS::ObjectValue(*members));
  JS::SetReservedSlot(event->object, kEventView, view);
  SetSlotInt(event->object, kEventUiDetail, detail);
  SetSlotInt(event->object, kEventModifiers, int32_t(modifiers));
  return true;
}
extern const JSFunctionSpec kTouchEventMethods[] = {
  JS_FN("initTouchEvent", InitTouchEvent, 1, JSPROP_ENUMERATE), JS_FS_END};
}
