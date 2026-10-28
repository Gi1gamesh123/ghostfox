#include "interaction/touch.h"
#include "interaction/event_keyboard.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::interaction {
bool InitTouchFields(JSContext* cx, JS::HandleObject dict, JS::HandleObject object) {
  State* state = RealmState(cx);
  if (!state) return Error(cx, "TypeError", "TouchEvent factory is not installed");
  int detail = 0;
  JS::RootedValue view(cx, JS::NullValue()), input(cx);
  // The donor EventInit contains this inherited boolean in addition to the
  // three standard base fields, already read by the original Construct path.
  bool ruyi = false;
  if (dict && (!ReadInitBool(cx, dict, "ruyi", &ruyi) ||
      !ReadInitInt(cx, dict, "detail", &detail) ||
      !JS_GetProperty(cx, dict, "view", &input) || !KeyboardView(cx, input, &view))) return false;
  uint32_t modifiers = 0;
  for (const auto& member : domain::kKeyboardModifiers) {
    bool enabled = false;
    if (dict && !ReadInitBool(cx, dict, member.member, &enabled)) return false;
    if (enabled) modifiers |= member.mask;
  }
  JS::RootedValueArray<3> lists(cx);
  const char* names[] = {"touches", "targetTouches", "changedTouches"};
  for (unsigned index : {2u, 1u, 0u}) {
    input.setUndefined();
    JS::RootedObject list(cx);
    if ((dict && !JS_GetProperty(cx, dict, names[index], &input)) ||
        !ConvertTouchSequence(*state, input, &list)) return false;
    lists[index].setObject(*list);
  }
  JS::RootedObject members(cx);
  if (!BuildTouchMembers(*state, lists, &members)) return false;
  const unsigned slots[] = {kEventTouches, kEventTargetTouches, kEventChangedTouches};
  for (unsigned i = 0; i < 3; ++i) JS::SetReservedSlot(object, slots[i], lists[i]);
  JS::SetReservedSlot(object, kTouchWidgetMembers, JS::ObjectValue(*members));
  JS::SetReservedSlot(object, kEventView, view);
  SetSlotInt(object, kEventUiDetail, detail);
  SetSlotInt(object, kEventModifiers, int32_t(modifiers));
  return true;
}
}
