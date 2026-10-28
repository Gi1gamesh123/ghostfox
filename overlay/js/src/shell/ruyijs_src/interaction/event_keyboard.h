#pragma once
#include "interaction/internal.h"
#include "domain/interaction/keyboard_values.h"

namespace ruyijs::interaction {
// The existing generic payload slots are unused by KeyboardEvent's ancestors.
inline constexpr unsigned kKeyboardCharCode = kEventPublicFirst;
inline constexpr unsigned kKeyboardKeyCode = kEventPublicSecond;
inline constexpr unsigned kKeyboardWhich = kEventPublicThird;
inline constexpr int32_t kKeyboardConstructed = 4;
bool InitKeyboardFields(JSContext*, JS::HandleObject, JS::HandleObject);
bool KeyboardView(JSContext*, JS::HandleValue, JS::MutableHandleValue);
bool KeyboardText(JSContext*, JS::HandleValue, JS::MutableHandleValue);
bool GetUiWhich(JSContext*, unsigned, JS::Value*);
bool GetKeyboardModifierState(JSContext*, unsigned, JS::Value*);
domain::KeyboardMessage KeyboardMessageOf(Event*);
uint32_t KeyboardCodeValue(JSContext*, Event*, domain::KeyboardCode);
bool InstallKeyEvent(State&);
}
