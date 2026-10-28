// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "interaction/event_keyboard.h"
#include "runtime/page_helpers.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::interaction {
extern const EventConstant kKeyboardConstants[] = {
  {"DOM_KEY_LOCATION_STANDARD", 0}, {"DOM_KEY_LOCATION_LEFT", 1},
  {"DOM_KEY_LOCATION_RIGHT", 2}, {"DOM_KEY_LOCATION_NUMPAD", 3},
#include "interaction/keyboard_constants.inc"
  {nullptr, 0}};
namespace {
bool IllegalKeyEvent(JSContext* cx, unsigned, JS::Value*) {
  return Error(cx, "TypeError", "Illegal constructor.");
}
}
bool InstallKeyEvent(State& state) {
  JSContext* cx = state.cx;
  JS::RootedFunction function(cx, JS_NewFunction(cx, IllegalKeyEvent, 0, JSFUN_CONSTRUCTOR, "KeyEvent"));
  JS::RootedObject object(cx, function ? JS_GetFunctionObject(function) : nullptr);
  if (!object) return false;
  const unsigned flags = JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;
  for (const EventConstant* entry = kKeyboardConstants + 4; entry->name; ++entry) {
    if (!JS_DefineProperty(cx, object, entry->name, entry->value, flags)) return false;
  }
  return JS_DefineProperty(cx, state.page, "KeyEvent", object, 0) &&
         state.helpers->Constructor("KeyEvent", object);
}
}
