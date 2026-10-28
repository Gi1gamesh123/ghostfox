// Host-originated trusted input (ARCHITECTURE.md, host user input). One event
// of the Mouse/Pointer, Keyboard, Input or Focus family is built from the
// natively installed interface prototype - never a page-replaceable global -
// and delivered on the target's resolved path, through the same factory and
// dispatch as every runtime event. The caller learns whether it was cancelled.
#include "interaction/internal.h"
#include "interaction/event_creation.h"
#include "interaction/realm.h"
#include "interaction/runtime_event.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include <cstring>

namespace ruyijs::interaction {
namespace {
struct Family { Iface iface; Kind kind; const char* name; };
bool Among(const char* type, std::initializer_list<const char*> names) {
  for (const char* name : names) if (!std::strcmp(type, name)) return true;
  return false;
}
bool Lookup(const char* type, Family* family) {
  if (Among(type, {"pointerdown", "pointermove", "pointerup", "pointercancel", "pointerover",
                   "pointerout", "pointerenter", "pointerleave", "click", "auxclick", "contextmenu"}))
    *family = {Iface::Pointer, Kind::Pointer, "PointerEvent"};
  else if (Among(type, {"mousedown", "mousemove", "mouseup", "dblclick", "mouseover",
                        "mouseout", "mouseenter", "mouseleave"}))
    *family = {Iface::Mouse, Kind::Mouse, "MouseEvent"};
  else if (Among(type, {"keydown", "keypress", "keyup"}))
    *family = {Iface::Keyboard, Kind::Keyboard, "KeyboardEvent"};
  else if (Among(type, {"beforeinput", "input"}))
    *family = {Iface::Input, Kind::Input, "InputEvent"};
  else if (Among(type, {"focus", "blur", "focusin", "focusout"}))
    *family = {Iface::Focus, Kind::Focus, "FocusEvent"};
  else return false;
  return true;
}
}
bool DispatchTrusted(JSContext* cx, JS::HandleObject target, const char* type,
                     JS::HandleObject init, double time_ms, bool* allowed) {
  JS::RootedObject local(cx, js::CheckedUnwrapStatic(target));
  State* state = local ? Owner(cx, local, false) : nullptr;
  if (!state) return Error(cx, "TypeError", "Input target must be a registered EventTarget");
  Family family;
  if (!Lookup(type, &family)) return Error(cx, "TypeError", "Unsupported trusted input type");
  JSAutoRealm realm(cx, state->page);
  JS::RootedObject dictionary(cx, init);
  if (!JS_WrapObject(cx, &dictionary)) return false;
  for (const char* field : {"bubbles", "cancelable", "composed"}) {
    bool present = false;
    if (!JS_HasOwnProperty(cx, dictionary, field, &present) ||
        (!present && !JS_DefineProperty(cx, dictionary, field, true, JSPROP_ENUMERATE))) return false;
  }
  if (!JS_DefineProperty(cx, dictionary, "view", state->page, JSPROP_ENUMERATE)) return false;
  JS::RootedObject prototype(cx);
  if (family.iface == Iface::Pointer) prototype = state->pointerPrototype;
  else if (family.iface == Iface::Mouse) prototype = state->mousePrototype;
  else if (!ReadInstalledEventPrototype(*state, family.iface, &prototype)) return false;
  return FireRuntimeEvent(*state, local, family.name, type, family.iface, family.kind,
                          dictionary, prototype, time_ms, true, allowed, true);
}
}  // namespace ruyijs::interaction
