#include "interaction/event_keyboard.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "js/Wrapper.h"

namespace ruyijs::interaction {
bool KeyboardView(JSContext* cx, JS::HandleValue input, JS::MutableHandleValue out) {
  out.setNull();
  if (input.isNullOrUndefined()) return true;
  JS::RootedObject object(cx, input.isObject() ? js::CheckedUnwrapStatic(&input.toObject()) : nullptr);
  for (State* state = activeState; object && state; state = state->previous) {
    if (state->cx == cx && state->page == object) { out.set(input); return true; }
  }
  return Error(cx, "TypeError", "KeyboardEvent view requires a Window");
}
bool KeyboardText(JSContext* cx, JS::HandleValue input, JS::MutableHandleValue out) {
  JS::RootedString text(cx, input.isUndefined() ? JS_GetEmptyString(cx) : JS::ToString(cx, input));
  if (!text) return false;
  if (JS_GetStringLength(text) > 1024 * 1024)
    return Error(cx, "RangeError", "KeyboardEvent text exceeds limit");
  out.setString(text);
  return true;
}
namespace {
bool ReadUnsigned(JSContext* cx, JS::HandleObject dict, const char* name, uint32_t* out) {
  *out = 0;
  JS::RootedValue value(cx);
  return !dict || (JS_GetProperty(cx, dict, name, &value) &&
      (value.isUndefined() || JS::ToUint32(cx, value, out)));
}
bool ReadText(JSContext* cx, JS::HandleObject dict, const char* name, JS::MutableHandleValue out) {
  JS::RootedValue input(cx);
  return (!dict || JS_GetProperty(cx, dict, name, &input)) && KeyboardText(cx, input, out);
}
}
bool InitKeyboardFields(JSContext* cx, JS::HandleObject dict, JS::HandleObject object) {
  int detail = 0;
  JS::RootedValue view(cx, JS::NullValue()), input(cx), key(cx), code(cx);
  if (dict && (!ReadInitInt(cx, dict, "detail", &detail) ||
      !JS_GetProperty(cx, dict, "view", &input) || !KeyboardView(cx, input, &view))) return false;
  uint32_t modifiers = 0;
  for (const auto& member : domain::kKeyboardModifiers) {
    bool enabled = false;
    if (dict && !ReadInitBool(cx, dict, member.member, &enabled)) return false;
    if (enabled) modifiers |= member.mask;
  }
  uint32_t character, key_code, location, which;
  bool composing = false, repeat = false;
  if (!ReadUnsigned(cx, dict, "charCode", &character) || !ReadText(cx, dict, "code", &code) ||
      (dict && !ReadInitBool(cx, dict, "isComposing", &composing)) ||
      !ReadText(cx, dict, "key", &key) || !ReadUnsigned(cx, dict, "keyCode", &key_code) ||
      !ReadUnsigned(cx, dict, "location", &location) ||
      (dict && !ReadInitBool(cx, dict, "repeat", &repeat)) ||
      !ReadUnsigned(cx, dict, "which", &which)) return false;
  SetSlotInt(object, kEventUiDetail, detail);
  JS::SetReservedSlot(object, kEventView, view);
  JS::SetReservedSlot(object, kEventKey, key);
  JS::SetReservedSlot(object, kEventCode, code);
  SetSlotNumber(object, kEventLocation, location);
  SetSlotNumber(object, kKeyboardCharCode, character);
  SetSlotNumber(object, kKeyboardKeyCode, key_code);
  SetSlotNumber(object, kKeyboardWhich, which);
  SetSlotInt(object, kEventModifiers, int32_t(modifiers));
  SetSlotInt(object, kEventKeyboardFlags, (repeat ? 1 : 0) | (composing ? 2 : 0) | kKeyboardConstructed);
  return true;
}
}
