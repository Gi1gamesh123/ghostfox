// The trusted `scroll` / `scrollend` UIEvents the Browser Environment's
// window scroll owner requests after the viewport moved (docs/owners/
// BROWSER_ENVIRONMENT.md, 2026-10-22). Firefox 158 values
// (artifacts/scroll-event): a UIEvent with view = the target's window,
// detail 0, bubbles, not composed; `scroll` is cancelable, `scrollend` is not.
// Built on the one runtime-event path from the realm's installed UIEvent
// prototype, never from a page-visible global.
#include "interaction/internal.h"
#include "interaction/event_creation.h"
#include "interaction/runtime_event.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include <cstring>

namespace ruyijs::interaction {
bool Realm::FireScroll(JS::HandleObject target, const char* type, bool bubbles) {
  State& state = *state_;
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.page);
  JS::RootedObject prototype(cx), init(cx, JS_NewPlainObject(cx));
  JS::RootedValue view(cx, JS::ObjectValue(*state.page));
  const bool cancelable = std::strcmp(type, "scroll") == 0;
  if (!init || !ReadInstalledEventPrototype(state, Iface::UI, &prototype) ||
      !JS_DefineProperty(cx, init, "bubbles", bubbles, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "cancelable", cancelable, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "composed", false, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "view", view, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, init, "detail", 0, JSPROP_ENUMERATE)) return false;
  return FireRuntimeEvent(state, target, "UIEvent", type, Iface::UI, Kind::UI, init, prototype);
}
}  // namespace ruyijs::interaction
