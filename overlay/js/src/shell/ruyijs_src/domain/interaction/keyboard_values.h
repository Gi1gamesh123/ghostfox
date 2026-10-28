#pragma once
#include "domain/interaction/dom_event.h"
#include <string_view>

namespace ruyijs::domain::interaction {
// KeyboardEventInit's inherited dictionary order. modifierOS is read by
// Firefox's binding but deliberately has no effect on modifier state.
inline constexpr uint32_t kFnLockMask = 1u << 13;
struct KeyboardModifier { const char* member; uint32_t mask; };
inline constexpr KeyboardModifier kKeyboardModifiers[] = {
    {"altKey", kAltMask}, {"ctrlKey", kControlMask}, {"metaKey", kMetaMask},
    {"modifierAltGraph", kAltGraphMask}, {"modifierCapsLock", kCapsLockMask},
    {"modifierFn", kFnMask}, {"modifierFnLock", kFnLockMask},
    {"modifierNumLock", kNumLockMask}, {"modifierOS", 0},
    {"modifierScrollLock", kScrollLockMask}, {"modifierSymbol", kSymbolMask},
    {"modifierSymbolLock", kSymbolLockMask}, {"shiftKey", kShiftMask}};
inline constexpr KeyboardModifier kModifierQueries[] = {
    {"Alt", kAltMask}, {"AltGraph", kAltGraphMask}, {"CapsLock", kCapsLockMask},
    {"Control", kControlMask}, {"Fn", kFnMask}, {"FnLock", kFnLockMask},
    {"Meta", kMetaMask}, {"NumLock", kNumLockMask}, {"ScrollLock", kScrollLockMask},
    {"Shift", kShiftMask}, {"Symbol", kSymbolMask}, {"SymbolLock", kSymbolLockMask},
    {"Accel", kControlMask}}; // Firefox's Windows accelerator is Control.

enum class KeyboardMessage { Other, Down, Up, Press };
enum class KeyboardCode { Character, Key, Which };
inline uint32_t KeyboardLegacyCode(KeyboardMessage message, KeyboardCode field,
                                   uint32_t character, uint32_t key) {
  if (message == KeyboardMessage::Other) return 0;
  if (field == KeyboardCode::Key) return key;
  if (field == KeyboardCode::Character)
    return message == KeyboardMessage::Press ? character : 0;
  return message != KeyboardMessage::Press || key == 13 || key == 8 ? key : character;
}
}
