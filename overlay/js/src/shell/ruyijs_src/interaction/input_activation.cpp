#include "interaction/internal.h"
#include "js/Object.h"
#include "js/String.h"
#include <cstring>

namespace ruyijs::interaction {
namespace {
bool Equals(JSContext* cx, JS::HandleObject event, uint32_t slot,
            const char* expected, bool* result) {
  JS::RootedValue value(cx, JS::GetReservedSlot(event, slot));
  if (!value.isString()) { *result = false; return true; }
  JS::RootedString text(cx, value.toString());
  return JS_StringEqualsAscii(cx, text, expected, result);
}
}
// Called once for DispatchTrusted, after native dictionary conversion and path
// preparation, before listeners. Read original slots, never replaceable getters.
bool NotifyInputActivation(State& state, JS::HandleObject target,
                           JS::HandleObject event, const char* type) {
  if (!state.inputActivation) return true;
  bool activate = false;
  if (!std::strcmp(type, "keydown")) {
    activate = true;
    // WidgetKeyboardEvent::GetModifierForKeyName plus Escape. Hyper and Super
    // are intentionally absent: the donor does not map them to a modifier.
    for (const char* name : {"Alt", "AltGraph", "CapsLock", "Control", "Fn", "FnLock",
                             "Meta", "NumLock", "ScrollLock", "Shift", "Symbol",
                             "SymbolLock", "Escape"}) {
      bool equal = false;
      if (!Equals(state.cx, event, kEventKey, name, &equal)) return false;
      if (equal) { activate = false; break; }
    }
  } else if (!std::strcmp(type, "pointerdown") || !std::strcmp(type, "pointerup")) {
    bool mouse = false;
    if (!Equals(state.cx, event, kEventPointerType, "mouse", &mouse)) return false;
    activate = !std::strcmp(type, "pointerdown") ? mouse : !mouse;
  }
  return !activate || state.inputActivation(state.inputActivationContext, target);
}
void Realm::SetInputActivationPort(InputActivationPort port, void* context) {
  state_->inputActivation = port;
  state_->inputActivationContext = context;
}
}
