#include "interaction/touch.h"
#include "js/PropertyAndElement.h"
#include "js/PropertyDescriptor.h"
#include "js/Realm.h"
#include "js/String.h"

namespace ruyijs::interaction {
bool CaptureTouchExposure(State& state) {
  JSContext* cx = state.cx;
  JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> desc(cx);
  if (!JS_GetOwnPropertyDescriptor(cx, state.page, "Touch", &desc) || desc.isNothing()) return false;
  state.touchDescriptor.set(*desc);
  if (!JS_GetOwnPropertyDescriptor(cx, state.page, "TouchList", &desc) || desc.isNothing()) return false;
  state.touchListDescriptor.set(*desc);
  if (!JS_GetOwnPropertyDescriptor(cx, state.page, "TouchEvent", &desc) || desc.isNothing()) return false;
  state.touchEventDescriptor.set(*desc);
  state.touchInstalled = state.touchEnabled = true;
  return true;
}
bool ApplyTouchExposure(State& state, bool enabled) {
  if (!state.touchInstalled || state.touchEnabled == enabled) return true;
  JSContext* cx = state.cx;
  const char* names[] = {"Touch", "TouchList", "TouchEvent"};
  const JS::PersistentRooted<JS::PropertyDescriptor>* saved[] = {
      &state.touchDescriptor, &state.touchListDescriptor, &state.touchEventDescriptor};
  for (unsigned i = 0; i < 3; ++i) {
    if (!enabled) {
      if (!JS_DeleteProperty(cx, state.page, names[i])) return false;
    } else {
      JS::RootedString name(cx, JS_AtomizeString(cx, names[i]));
      JS::RootedId id(cx);
      JS::Rooted<JS::PropertyDescriptor> descriptor(cx, saved[i]->get());
      if (!name || !JS_StringToId(cx, name, &id) ||
          !JS_DefinePropertyById(cx, state.page, id, descriptor)) return false;
    }
  }
  state.touchEnabled = enabled;
  return true;
}
bool Realm::ConfigureTouchExposure(bool enabled) {
  State& state = *state_;
  JSAutoRealm enter(state.cx, state.page);
  return ApplyTouchExposure(state, enabled);
}
}
