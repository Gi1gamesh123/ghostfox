#include "interaction/touch.h"
#include "js/Object.h"
#include "js/Realm.h"

namespace ruyijs::interaction {
namespace {
bool List(JSContext* cx, unsigned argc, JS::Value* vp, unsigned slot, const char* name) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::Touch, true);
  if (!event) return false;
  ReportEventAccess(cx, event, "TouchEvent", name, int(ApiAccess::Read));
  JS::RootedObject list(cx);
  if (!TouchEventList(cx, event, slot, &list)) return false;
  if (slot == kEventTargetTouches) {
    State* owner = Owner(cx, event->object, true);
    JS::RootedObject target(cx, owner->targetObject(event->value.target_id()));
    if (target && !PrepareTouchTargetList(cx, event, target)) return false;
  }
  args.rval().setObject(*list);
  return JS_WrapValue(cx, args.rval());
}
bool Flag(JSContext* cx, unsigned argc, JS::Value* vp, uint32_t mask, const char* name) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::Touch, true);
  if (!event) return false;
  ReportEventAccess(cx, event, "TouchEvent", name, int(ApiAccess::Read));
  args.rval().setBoolean((uint32_t(JS::GetReservedSlot(event->object, kEventModifiers).toInt32()) & mask) != 0);
  return true;
}
#define TOUCH_LIST(fn, slot, name) \
  bool fn(JSContext* cx, unsigned argc, JS::Value* vp) { return List(cx, argc, vp, slot, name); }
TOUCH_LIST(Touches, kEventTouches, "touches")
TOUCH_LIST(TargetTouches, kEventTargetTouches, "targetTouches")
TOUCH_LIST(ChangedTouches, kEventChangedTouches, "changedTouches")
#undef TOUCH_LIST
#define TOUCH_FLAG(fn, mask, name) \
  bool fn(JSContext* cx, unsigned argc, JS::Value* vp) { return Flag(cx, argc, vp, mask, name); }
TOUCH_FLAG(Alt, domain::kAltMask, "altKey")
TOUCH_FLAG(Meta, domain::kMetaMask, "metaKey")
TOUCH_FLAG(Control, domain::kControlMask, "ctrlKey")
TOUCH_FLAG(Shift, domain::kShiftMask, "shiftKey")
#undef TOUCH_FLAG
}
extern const JSPropertySpec kTouchProperties[] = {
  JS_PSG("touches", Touches, JSPROP_ENUMERATE),
  JS_PSG("targetTouches", TargetTouches, JSPROP_ENUMERATE),
  JS_PSG("changedTouches", ChangedTouches, JSPROP_ENUMERATE),
  JS_PSG("altKey", Alt, JSPROP_ENUMERATE), JS_PSG("metaKey", Meta, JSPROP_ENUMERATE),
  JS_PSG("ctrlKey", Control, JSPROP_ENUMERATE), JS_PSG("shiftKey", Shift, JSPROP_ENUMERATE), JS_PS_END};
}
