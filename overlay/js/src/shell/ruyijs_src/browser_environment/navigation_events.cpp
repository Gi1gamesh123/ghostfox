// The Navigation events of a same-document commit (docs/owners/BROWSER_ENVIRONMENT.md,
// 2026-10-21): a trusted cancelable NavigateEvent before the commit, then
// NavigationCurrentEntryChangeEvent and navigatesuccess after it. The events are
// built and dispatched by the Interaction owner; this file only supplies the
// init dictionaries and the session-history references.
#include "browser_environment/navigation_api.h"
#include "interaction/realm.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::browser_environment {
namespace {
bool Put(JSContext* cx, JS::HandleObject init, const char* name, JS::HandleValue value) {
  return JS_DefineProperty(cx, init, name, value, JSPROP_ENUMERATE);
}
bool PutType(JSContext* cx, JS::HandleObject init, const char* type) {
  JS::RootedValue v(cx, JS::NullValue());
  if (type) {
    JSString* s = JS_NewStringCopyZ(cx, type);
    if (!s) return false;
    v.setString(s);
  }
  return Put(cx, init, "navigationType", v);
}
}  // namespace

bool NavigationBeforeCommit(State& state, const char* type, const std::string& url, bool hashChange,
                            JS::HandleValue info, JS::HandleValue navigationState,
                            JS::MutableHandleValue from, bool* proceed) {
  *proceed = true;
  from.setNull();
  if (!state.navigation || state.shared) return true;
  NavigationState& n = *state.navigation;
  JSContext* cx = state.cx;
  JS::RootedObject init(cx, JS_NewPlainObject(cx)), destination(cx), target(cx, n.instance);
  JS::RootedValue v(cx);
  if (!init || !NavigationCurrentEntry(n, from) ||
      !NewNavigationDestination(n, url, true, navigationState, &destination)) return false;
  v.setObject(*destination);
  if (!PutType(cx, init, type) || !Put(cx, init, "destination", v)) return false;
  v.setBoolean(true);
  if (!Put(cx, init, "canIntercept", v) || !Put(cx, init, "cancelable", v)) return false;
  v.setBoolean(false);
  if (!Put(cx, init, "userInitiated", v)) return false;
  v.setBoolean(hashChange);
  if (!Put(cx, init, "hashChange", v) || !Put(cx, init, "info", info)) return false;
  return n.events.FireNavigate(target, init, proceed);
}

bool NavigationFireChange(NavigationState& n, const char* type, JS::HandleValue from) {
  JSContext* cx = n.env.cx;
  JS::RootedObject init(cx, JS_NewPlainObject(cx)), target(cx, n.instance);
  if (!init || !PutType(cx, init, type) || !Put(cx, init, "from", from)) return false;
  return n.events.FireNavigationEntryChange(target, init);
}

bool NavigationAfterCommit(State& state, const char* type, JS::HandleValue from) {
  if (!state.navigation || state.shared) return true;
  NavigationState& n = *state.navigation;
  JS::RootedObject target(state.cx, n.instance);
  return NavigationFireChange(n, type, from) && n.events.Fire(target, "navigatesuccess", false, false);
}
}  // namespace ruyijs::browser_environment
