// The queued CookieChangeEvent of window.cookieStore. The jar reports what one
// assignment changed (model_storage.h CookieChangeSet); this builds the two
// lists the interface returns and posts one realm task that fires the trusted
// event at the realm's CookieStore object, as Firefox's "fire a cookie change
// event" step does. The event object, its slots and its delivery stay the
// Interaction owner's one implementation (interaction/realm.h FireCookieChange).
#include "resource_network/page_cookie_store.h"
#include "resource_network/state.h"
#include "interaction/realm.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::resource_network {
namespace {
JSObject* List(JSContext* cx, const std::vector<CookieEntry>& entries, bool withValue) {
  JS::RootedObject list(cx, JS::NewArrayObject(cx, 0));
  if (!list) return nullptr;
  uint32_t index = 0;
  for (const auto& entry : entries) {
    JS::RootedObject item(cx, CookieItem(cx, entry, withValue));
    if (!item || !JS_SetElement(cx, list, index++, item)) return nullptr;
  }
  return list;
}
// The task body: fire the change the jar reported, in this realm.
bool Fire(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject store(cx, &js::GetFunctionNativeReserved(&args.callee(), 0).toObject());
  auto* s = static_cast<State*>(JS::GetReservedSlot(store, kStoreOwner).toPrivate());
  JS::RootedValue changed(cx, js::GetFunctionNativeReserved(&args.callee(), 1));
  JS::RootedValue deleted(cx, js::GetFunctionNativeReserved(&args.callee(), 2));
  args.rval().setUndefined();
  return s->events && s->events->FireCookieChange(store, changed, deleted);
}
}  // namespace

bool PostCookieChange(State& s, const CookieChangeSet& change) {
  if ((change.changed.empty() && change.deleted.empty()) || !s.cookieTask || !s.cookieStore)
    return true;
  JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.page);
  JS::RootedObject changed(cx, List(cx, change.changed, true));
  JS::RootedObject deleted(cx, List(cx, change.deleted, false));
  JS::RootedFunction function(cx, js::NewFunctionWithReserved(cx, Fire, 0, 0, nullptr));
  if (!changed || !deleted || !function) return false;
  JS::RootedObject task(cx, JS_GetFunctionObject(function));
  js::SetFunctionNativeReserved(task, 0, JS::ObjectValue(*s.cookieStore));
  js::SetFunctionNativeReserved(task, 1, JS::ObjectValue(*changed));
  js::SetFunctionNativeReserved(task, 2, JS::ObjectValue(*deleted));
  return s.cookieTask(s.cookieTaskContext, task);
}
}  // namespace ruyijs::resource_network
