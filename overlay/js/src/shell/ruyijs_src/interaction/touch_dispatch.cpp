#include "interaction/touch.h"
#include "interaction/touch_dispatch.h"
#include "js/Array.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::interaction {
void ApplyTouchDispatch(Event& event, TouchDispatchData& data, size_t position) {
  if (!data.active) return;
  const size_t count = data.members.length(), offset = position * count;
  bool changed = false;
  for (size_t i = 0; i < count; ++i) {
    if (data.started && data.targets[offset + i] != data.targets[data.lastOffset + i]) changed = true;
  }
  if (!data.started || changed) for (size_t i = 0; i < count; ++i)
    JS::SetReservedSlot(data.members[i], kTouchTarget,
                        JS::ObjectOrNullValue(data.targets[offset + i]));
  const auto cached = JS::GetReservedSlot(event.object, kEventTargetTouches);
  JSObject* list = cached.isObject() ? js::UncheckedUnwrap(&cached.toObject()) : nullptr;
  if (list && ((!data.started && data.initialPresent) || changed))
    js::SetProxyReservedSlot(list, 0, JS::ObjectValue(*data.emptyBackings[data.nextEmpty++]));
  if (position == 0 && data.initialPresent && (!data.started || data.lastOffset != 0)) {
    // Target capture and target bubble share the same mutable internal backing.
    js::SetProxyReservedSlot(list, 0, JS::ObjectValue(*data.initialBacking));
    for (size_t i = 0; i < data.initialMembers.length(); ++i)
      JS::SetReservedSlot(data.initialMembers[i], kTouchTarget,
                          JS::ObjectOrNullValue(data.initialTargets[i]));
  }
  data.lastOffset = offset;
  data.started = true;
}
bool PrepareTouchTargetList(JSContext* cx, Event* event, JS::HandleObject target) {
  JS::RootedObject list(cx);
  uint32_t existing;
  if (!TouchEventList(cx, event, kEventTargetTouches, &list) ||
      !TouchListLength(cx, list, &existing)) return false;
  if (existing || !target) return true;
  JS::RootedObject expected(cx, js::CheckedUnwrapStatic(target));
  JS::RootedObject members(cx, &JS::GetReservedSlot(event->object, kTouchWidgetMembers).toObject());
  JSAutoRealm enter(cx, members);
  uint32_t length;
  if (!JS::GetArrayLength(cx, members, &length)) return false;
  for (uint32_t i = 0; i < length; ++i) {
    JS::RootedValue member(cx);
    if (!JS_GetElement(cx, members, i, &member)) return false;
    JS::RootedObject touch(cx, TouchReceiver(cx, member));
    if (!touch) return false;
    JS::RootedValue current(cx, JS::GetReservedSlot(touch, kTouchTarget));
    JSObject* actual = current.isObject() ? js::CheckedUnwrapStatic(&current.toObject()) : nullptr;
    if (actual == expected && !AppendTouch(cx, list, member)) return false;
  }
  return true;
}
}
