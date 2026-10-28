#include "interaction/touch.h"
#include "interaction/touch_dispatch.h"
#include "js/Array.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::interaction {
bool PrepareTouchDispatch(JSContext* cx, Event& event, TouchDispatchData& data) {
  if (event.kind != Kind::Touch) return true;
  data.active = true;
  JSAutoRealm enter(cx, event.object);
  JS::RootedObject members(cx, &JS::GetReservedSlot(event.object, kTouchWidgetMembers).toObject());
  uint32_t length;
  if (!JS::GetArrayLength(cx, members, &length)) return false;
  for (uint32_t i = 0; i < length; ++i) {
    JS::RootedValue member(cx);
    if (!JS_GetElement(cx, members, i, &member)) return false;
    JS::RootedObject touch(cx, TouchReceiver(cx, member));
    if (!touch) return false;
    JS::RootedValue original(cx, JS::GetReservedSlot(touch, kTouchOriginalTarget));
    JS::RootedObject target(cx, js::CheckedUnwrapStatic(&original.toObject()));
    if (!target) return Error(cx, "TypeError", "Touch original target is unavailable");
    if (!data.members.append(touch) || !data.originalTargets.append(target)) {
      JS_ReportOutOfMemory(cx); return false;
    }
  }
  JS::RootedValue cached(cx, JS::GetReservedSlot(event.object, kEventTargetTouches));
  data.initialPresent = cached.isObject();
  data.list = cached.isObject() ? TouchListReceiver(cx, cached) : nullptr;
  JS::RootedObject factory(cx, data.list ? data.list.get() : event.object.get());
  JSAutoRealm list_realm(cx, factory);
  data.initialBacking = JS::NewArrayObject(cx, 0);
  if (!data.initialBacking) return false;
  if (!data.initialPresent) return true;
  if (!data.list || !TouchListLength(cx, data.list, &length)) return false;
  for (uint32_t i = 0; i < length; ++i) {
    JS::RootedValue member(cx);
    if (!TouchListItem(cx, data.list, i, &member) ||
        !JS_DefineElement(cx, data.initialBacking, i, member, JSPROP_ENUMERATE)) return false;
    JS::RootedObject touch(cx, TouchReceiver(cx, member));
    if (!touch) return false;
    JS::RootedObject target(cx, &JS::GetReservedSlot(touch, kTouchOriginalTarget).toObject());
    if (!data.initialMembers.append(touch) || !data.initialTargets.append(target)) {
      JS_ReportOutOfMemory(cx); return false;
    }
  }
  return true;
}
bool CompleteTouchDispatch(JSContext* cx, TouchDispatchData& data, size_t positions) {
  if (!data.active) return true;
  const size_t count = data.members.length();
  if (positions > 100000 || (count && positions > 1000000 / count) ||
      data.targets.length() != count * positions)
    return Error(cx, "RangeError", "Touch dispatch projection exceeds its bounded path");
  for (size_t position = 0; position < positions; ++position) {
    for (size_t i = 0; i < count; ++i) {
      JSAutoRealm enter(cx, data.members[i]);
      JS::RootedObject target(cx, data.targets[position * count + i]);
      if (target && !JS_WrapObject(cx, &target)) return false;
      data.targets[position * count + i].set(target);
    }
  }
  size_t changes = 0;
  for (size_t position = 1; position < positions; ++position) {
    for (size_t i = 0; i < count; ++i)
      if (data.targets[position * count + i] != data.targets[(position - 1) * count + i]) {
        ++changes; break;
      }
  }
  // Same realm as an existing foreign legacy list, or the future factory list.
  JSAutoRealm enter(cx, data.initialBacking);
  for (size_t i = 0; i < changes * 2 + 1; ++i) {
    JS::RootedObject backing(cx, JS::NewArrayObject(cx, 0));
    if (!backing || !data.emptyBackings.append(backing)) {
      if (backing) JS_ReportOutOfMemory(cx);
      return false;
    }
  }
  return true;
}
}
