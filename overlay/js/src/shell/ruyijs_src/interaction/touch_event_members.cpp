#include "interaction/touch.h"
#include "js/Array.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::interaction {
bool BuildTouchMembers(State& state, const JS::HandleValueArray& lists,
                        JS::MutableHandleObject result) {
  JSContext* cx = state.cx;
  JSAutoRealm enter(cx, state.page);
  result.set(JS::NewArrayObject(cx, 0));
  if (!result) return false;
  uint32_t count = 0;
  for (unsigned source : {1u, 0u, 2u}) {
    if (!lists[source].isObject()) continue;
    JS::RootedObject list(cx, TouchListReceiver(cx, lists[source]));
    uint32_t length;
    if (!list || !TouchListLength(cx, list, &length)) return false;
    for (uint32_t i = 0; i < length; ++i) {
      JS::RootedValue member(cx), existing(cx);
      if (!TouchListItem(cx, list, i, &member)) return false;
      bool duplicate = false;
      if (source != 1) for (uint32_t j = 0; j < count; ++j) {
        if (!JS_GetElement(cx, result, j, &existing)) return false;
        if (existing.get() == member.get()) { duplicate = true; break; }
      }
      if (!duplicate && !JS_DefineElement(cx, result, count++, member, JSPROP_ENUMERATE)) return false;
    }
  }
  return true;
}
bool TouchEventList(JSContext* cx, Event* event, uint32_t slot,
                     JS::MutableHandleObject result) {
  State* owner = Owner(cx, event->object, true);
  if (!owner) return Error(cx, "TypeError", "TouchEvent owner is not installed");
  {
    JSAutoRealm enter(cx, owner->page);
    JS::RootedValue cached(cx, JS::GetReservedSlot(event->object, slot));
    if (!cached.isObject()) {
      if (!NewTouchList(*owner, result)) return false;
      if (slot == kEventTouches) {
        JS::RootedObject members(cx, &JS::GetReservedSlot(event->object, kTouchWidgetMembers).toObject());
        uint32_t length;
        if (!JS::GetArrayLength(cx, members, &length)) return false;
        for (uint32_t i = 0; i < length; ++i) {
          JS::RootedValue member(cx);
          if (!JS_GetElement(cx, members, i, &member) || !AppendTouch(cx, result, member)) return false;
        }
      }
      cached.setObject(*result);
      JS::SetReservedSlot(event->object, slot, cached);
    }
    result.set(TouchListReceiver(cx, cached));
    if (!result) return false;
  }
  return true;
}
}
