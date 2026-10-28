// The one production installer of Navigation, its entry interfaces and
// window.navigation (docs/owners/BROWSER_ENVIRONMENT.md, 2026-10-21). Each
// window realm gets its own NavigationState, instance and entry objects.
#include "browser_environment/navigation_api.h"
#include "interaction/realm.h"
#include "runtime/page_helpers.h"
#include "js/MapAndSet.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::browser_environment {
const char* const kNavigationMemberNames[kNavigationMemberCount] = {
    "entries", "updateCurrentEntry", "navigate", "reload", "traverseTo", "back", "forward",
    "currentEntry", "transition", "activation", "canGoBack", "canGoForward"};
const char* const kNavigationHandlerTypes[4] = {"navigate", "navigatesuccess", "navigateerror",
                                                "currententrychange"};

// The current document's activation: a push into the entry that document was
// loaded into (the nearest non-same-document entry at or before the current
// one). Built on first read per document, since one realm may host successive
// documents; a child about:blank document has no entries and no activation.
bool NavigationActivationValue(NavigationState& n, const std::vector<Model::HistoryView>& views, int32_t index,
                               JS::MutableHandleValue out) {
  JSContext* cx = n.env.cx;
  out.setNull();
  if (index < 0) return true;
  size_t document = size_t(index);
  while (document > 0 && views[document].sameDocument) --document;
  if (n.activationObject && n.activationDocument == views[document].id) {
    out.setObject(*n.activationObject);
    return true;
  }
  JS::RootedObject proto(cx, n.activation.prototype), entry(cx);
  if (!NavigationEntryObject(n, views[document], &entry)) return false;
  JSString* push = JS_NewStringCopyZ(cx, "push");
  n.activationObject = n.kit.New(&kNavigationActivationClass, proto);
  if (!n.activationObject || !push) return false;
  JS::SetReservedSlot(n.activationObject, 0, JS::NullValue());
  JS::SetReservedSlot(n.activationObject, 1, JS::ObjectValue(*entry));
  JS::SetReservedSlot(n.activationObject, 2, JS::StringValue(push));
  n.activationDocument = views[document].id;
  out.setObject(*n.activationObject);
  return true;
}

bool InstallNavigation(State& state, interaction::Realm& events) {
  if (state.worker) return true;  // [Exposed=Window]
  JSContext* cx = state.cx;
  state.navigation = std::make_shared<NavigationState>(state, events);
  NavigationState& n = *state.navigation;
  JS::RootedObject target(cx, events.prototype());
  if (!InstallNavigationEntries(n) || !InstallNavigationPrecommit(n) ||
      !n.kit.Define("Navigation", &kNavigationClass, target, WebKit::Reject, &n.navigation)) return false;
  const unsigned lengths[] = {0, 1, 1, 0, 1, 0, 0};
  for (int m = mEntries; m <= mForward; ++m)
    if (!n.kit.Method(n.navigation, "Navigation", kNavigationMemberNames[m], NavigationMember, lengths[m], m))
      return false;
  for (int m = mCurrent; m <= mCanForward; ++m)
    if (!n.kit.Accessor(n.navigation, "Navigation", kNavigationMemberNames[m], NavigationMember, nullptr, m))
      return false;
  JS::RootedObject proto(cx, n.navigation.prototype);
  for (int i = 0; i < 4; ++i) {
    const std::string name = std::string("on") + kNavigationHandlerTypes[i];
    JS::RootedObject get(cx, n.kit.Function(NavigationMember, ("get " + name).c_str(), 0, 50 + i));
    JS::RootedObject set(cx, n.kit.Function(NavigationMember, ("set " + name).c_str(), 1, 150 + i));
    if (!get || !set || !JS_DefineProperty(cx, proto, name.c_str(), get, set, JSPROP_ENUMERATE) ||
        !state.helpers->Accessor("Navigation", proto, name.c_str())) return false;
  }
  n.instance = n.kit.New(&kNavigationClass, proto);
  n.entries = JS::NewMapObject(cx);
  if (!n.instance || !n.entries || !events.Attach(n.instance)) return false;
  // window.navigation: [Replaceable] readonly attribute.
  JS::RootedObject page(cx, state.page);
  JS::RootedObject get(cx, n.kit.Function(NavigationWindowAccessor, "get navigation", 0, 0));
  JS::RootedObject set(cx, n.kit.Function(NavigationWindowAccessor, "set navigation", 1, 1));
  return get && set &&
         JS_DefineProperty(cx, page, "navigation", get, set, JSPROP_ENUMERATE) &&
         state.helpers->Accessor("Window", page, "navigation");
}
}  // namespace ruyijs::browser_environment
