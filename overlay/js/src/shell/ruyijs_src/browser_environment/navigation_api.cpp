// The Navigation interface and window.navigation (docs/owners/BROWSER_ENVIRONMENT.md,
// 2026-10-21). Same-document navigate() commits through the one session history;
// cross-document navigate()/reload() use the original PageLoad navigate port and
// back/forward/traverseTo the original History traversal task.
#include "browser_environment/navigation_api.h"
#include "browser_environment/window_members.h"
#include "runtime/runtime.h"
#include "domain/browser_environment/url_value.h"
#include "interaction/realm.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/MapAndSet.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::browser_environment {
const JSClass kNavigationClass = {"Navigation", JSCLASS_HAS_RESERVED_SLOTS(1)};
NavigationState::NavigationState(State& e, interaction::Realm& ev)
    : env(e), events(ev), kit(e.cx, e.page, *e.helpers, this), navigation(e.cx), entry(e.cx), activation(e.cx),
      transition(e.cx), destination(e.cx), instance(e.cx), activationObject(e.cx), entries(e.cx),
      navigateEvent(e.cx), changeEvent(e.cx), abortController(e.cx),
      handlers{JS::PersistentRootedValue(e.cx), JS::PersistentRootedValue(e.cx), JS::PersistentRootedValue(e.cx),
               JS::PersistentRootedValue(e.cx)} {}
namespace {
using UrlValue = ruyijs::domain::browser_environment::UrlValue;

// { committed, finished }: both settled with `entry`, both rejected with `error`, or both pending.
bool Result(NavigationState& n, JS::HandleValue entry, JS::HandleValue error, JS::MutableHandleValue out) {
  JSContext* cx = n.env.cx;
  JS::RootedObject result(cx, JS_NewPlainObject(cx));
  if (!result) return false;
  for (const char* name : {"committed", "finished"}) {
    JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
    if (!promise) return false;
    if (!error.isUndefined()) {
      if (!JS::RejectPromise(cx, promise, error) || !JS::SetSettledPromiseIsHandled(cx, promise)) return false;
    } else if (!entry.isUndefined() && !JS::ResolvePromise(cx, promise, entry)) {
      return false;
    }
    JS::RootedValue v(cx, JS::ObjectValue(*promise));
    if (!JS_DefineProperty(cx, result, name, v, JSPROP_ENUMERATE)) return false;
  }
  out.setObject(*result);
  return true;
}
bool DomError(NavigationState& n, const char* name, const char* message, JS::MutableHandleValue out) {
  if (ThrowDOM(n.env, name, message)) return false;
  return JS_GetPendingException(n.env.cx, out) && (JS_ClearPendingException(n.env.cx), true);
}
bool Traverse(NavigationState& n, int32_t delta, const char* message, JS::MutableHandleValue rval) {
  JS::RootedValue error(n.env.cx), none(n.env.cx);
  if (!delta || !n.env.postTask) {
    if (!DomError(n, "InvalidStateError", message, &error)) return false;
    return Result(n, none, error, rval);
  }
  // The original History traversal task (history.cpp): commit, URL, popstate.
  JSFunction* deliver = js::NewFunctionWithReserved(n.env.cx, DeliverPopState, 0, 0, "popstate");
  if (!deliver) return false;
  JS::RootedObject task(n.env.cx, JS_GetFunctionObject(deliver));
  js::SetFunctionNativeReserved(task, 0, JS::PrivateValue(&n.env));
  js::SetFunctionNativeReserved(task, 1, JS::Int32Value(delta));
  return n.env.postTask(n.env.postTaskContext, task) && Result(n, none, none, rval);
}
bool Navigate(NavigationState& n, const JS::CallArgs& args) {
  JSContext* cx = n.env.cx;
  if (!args.length()) return WebKit::Fail(cx, "Navigation.navigate: At least 1 argument required, but only 0 passed");
  std::string input, current, history = "auto";
  JS::RootedString text(cx, JS::ToString(cx, args[0]));
  JS::RootedValue info(cx), state(cx), v(cx), none(cx), error(cx), entry(cx);
  if (!text || !ToUTF8(cx, text, &input)) return false;
  if (!args.get(1).isNullOrUndefined()) {
    if (!args[1].isObject()) return WebKit::Fail(cx, "Navigation.navigate: Argument 2 can't be converted to a dictionary.");
    JS::RootedObject o(cx, &args[1].toObject());
    if (!JS_GetProperty(cx, o, "history", &v)) return false;
    if (!v.isUndefined()) {
      JS::RootedString h(cx, JS::ToString(cx, v));
      if (!h || !ToUTF8(cx, h, &history)) return false;
      if (history != "auto" && history != "push" && history != "replace")
        return WebKit::Fail(cx, ("Navigation.navigate: '" + history + "' (value of 'history' member of "
                                 "NavigationNavigateOptions) is not a valid value for enumeration "
                                 "NavigationHistoryBehavior.").c_str());
    }
    if (!JS_GetProperty(cx, o, "info", &info) || !JS_GetProperty(cx, o, "state", &state)) return false;
  }
  if (!n.env.model.HistoryCurrentUrl(&current)) { JS_ReportOutOfMemory(cx); return false; }
  UrlValue target;
  if (!UrlValue::Resolve(input, current, &target)) {
    if (!DomError(n, "SyntaxError", ("Navigation.navigate: Invalid URL '" + input + "'").c_str(), &error)) return false;
    return Result(n, none, error, args.rval());
  }
  std::vector<uint8_t> bytes;
  if (!state.isUndefined() && !EncodeState(n.env, state, &bytes)) return false;
  const std::string href = target.href();
  const bool replace = history == "replace";
  const auto strip = [](const std::string& url) { return url.substr(0, url.find('#')); };
  if (strip(href) != strip(current) || href.find('#') == std::string::npos) {  // cross-document
    if (n.env.shared || !n.env.navigate) return Result(n, none, none, args.rval());
    return n.env.navigate(n.env.navigateContext, href.c_str(), replace, false) && Result(n, none, none, args.rval());
  }
  bool proceed = true;
  const char* type = replace ? "replace" : "push";
  JS::RootedValue from(cx);
  if (!NavigationBeforeCommit(n.env, type, href, true, info, state, &from, &proceed)) return false;
  if (!proceed) {
    if (!DomError(n, "AbortError", "Navigation was aborted", &error)) return false;
    return Result(n, none, error, args.rval());
  }
  if (!n.env.model.HistoryCommit(replace, href, {}, bytes)) { JS_ReportOutOfMemory(cx); return false; }
  return NavigationAfterCommit(n.env, type, from) && NavigationCurrentEntry(n, &entry) &&
         Result(n, entry, none, args.rval());
}
}  // namespace
bool NavigationMember(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NavigationState& n = *static_cast<NavigationState*>(WebKit::Owner(args));
  const int slot = WebKit::Slot(args), member = slot % 100;
  JSObject* self = args.thisv().isObject() ? &args.thisv().toObject() : nullptr;
  if (!self || JS::GetClass(self) != &kNavigationClass) {
    const bool handler = slot >= 50;
    const std::string name = handler ? std::string(slot >= 100 ? "set on" : "get on") + kNavigationHandlerTypes[member - 50]
        : member >= mCurrent ? std::string("get ") + kNavigationMemberNames[member] : std::string(kNavigationMemberNames[member]);
    return WebKit::Fail(cx, ("'" + name + "' called on an object that does not implement interface Navigation.").c_str());
  }
  std::vector<Model::HistoryView> views;
  int32_t index = -1;
  JS::RootedValue none(cx), from(cx);
  JS::MutableHandleValue rval = args.rval();
  if (slot >= 50) {  // on* handlers: one EventTarget registration through the shared methods
    JS::PersistentRootedValue& stored = n.handlers[member - 50];
    if (slot < 100) { rval.set(stored); return true; }
    JS::RootedValue previous(cx, stored), desired(cx, args.get(0)), method(cx), ignored(cx);
    if (!desired.isObject() || !JS::IsCallable(&desired.toObject())) desired.setNull();
    JS::RootedObject proto(cx, n.events.prototype()), target(cx, self);
    JS::RootedValueArray<2> params(cx);
    JS::RootedString type(cx, JS_NewStringCopyZ(cx, kNavigationHandlerTypes[member - 50]));
    if (!type) return false;
    params[0].setString(type);
    for (int add = 0; add < 2; ++add) {
      params[1].set(add ? desired : previous);
      if (params[1].isObject() && (!JS_GetProperty(cx, proto, add ? "addEventListener" : "removeEventListener", &method) ||
                                   !JS::Call(cx, JS::RootedValue(cx, JS::ObjectValue(*target)), method, params, &ignored))) return false;
    }
    stored.set(desired);
    rval.setUndefined();
    return true;
  }
  if (!NavigationEntries(n, &views, &index)) return false;
  switch (member) {
    case mEntries: {
      JS::RootedObject array(cx, JS::NewArrayObject(cx, 0)), entry(cx);
      if (!array) return false;
      for (size_t i = 0; i < views.size(); ++i) {
        JS::RootedValue item(cx);
        if (!NavigationEntryObject(n, views[i], &entry)) return false;
        item.setObject(*entry);
        if (!JS_DefineElement(cx, array, uint32_t(i), item, JSPROP_ENUMERATE)) return false;
      }
      rval.setObject(*array);
      return true;
    }
    case mUpdate: {
      if (!argc) return WebKit::Fail(cx, "Navigation.updateCurrentEntry: At least 1 argument required, but only 0 passed");
      if (!args[0].isObject()) return WebKit::Fail(cx, "Navigation.updateCurrentEntry: Argument 1 can't be converted to a dictionary.");
      JS::RootedObject o(cx, &args[0].toObject());
      JS::RootedValue state(cx);
      std::vector<uint8_t> bytes;
      if (!JS_GetProperty(cx, o, "state", &state)) return false;
      if (state.isUndefined()) return WebKit::Fail(cx, "Navigation.updateCurrentEntry: Missing required 'state' member of NavigationUpdateCurrentEntryOptions.");
      if (index < 0) return ThrowDOM(n.env, "InvalidStateError", "Navigation.updateCurrentEntry: No current entry");
      if (!EncodeState(n.env, state, &bytes) || !NavigationCurrentEntry(n, &from)) return false;
      if (!n.env.model.SetHistoryNavigationState(bytes)) { JS_ReportOutOfMemory(cx); return false; }
      rval.setUndefined();
      return NavigationFireChange(n, nullptr, from);
    }
    case mNavigate: return Navigate(n, args);
    case mReload: {
      std::string url;
      if (!n.env.model.HistoryCurrentUrl(&url)) { JS_ReportOutOfMemory(cx); return false; }
      if (!n.env.shared && n.env.navigate && !n.env.navigate(n.env.navigateContext, url.c_str(), true, true)) return false;
      return Result(n, none, none, rval);
    }
    case mTraverse: {
      if (!argc) return WebKit::Fail(cx, "Navigation.traverseTo: At least 1 argument required, but only 0 passed");
      JS::RootedString key(cx, JS::ToString(cx, args[0]));
      std::string wanted;
      if (!key || !ToUTF8(cx, key, &wanted)) return false;
      for (size_t i = 0; i < views.size(); ++i)
        if (n.keys.count(views[i].id) && n.keys[views[i].id] == wanted)
          return Traverse(n, int32_t(i) - index, "Navigation.traverseTo: Invalid key", rval);
      return Traverse(n, 0, "Navigation.traverseTo: Invalid key", rval);
    }
    case mBack: return Traverse(n, index > 0 ? -1 : 0, "Navigation.back: Cannot go back", rval);
    case mForward:
      return Traverse(n, index >= 0 && index + 1 < int32_t(views.size()) ? 1 : 0, "Navigation.forward: Cannot go forward", rval);
    case mCurrent: return NavigationCurrentEntry(n, rval);
    case mTransition: rval.setNull(); return true;
    case mActivation: return NavigationActivationValue(n, views, index, rval);
    case mCanBack: rval.setBoolean(index > 0); return true;
    default: rval.setBoolean(index >= 0 && index + 1 < int32_t(views.size())); return true;
  }
}
bool NavigationWindowAccessor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NavigationState& n = *static_cast<NavigationState*>(WebKit::Owner(args));
  if (WebKit::Slot(args) == 1) {  // [Replaceable]: the setter shadows with an own data property
    JS::RootedObject page(cx, n.env.page);
    args.rval().setUndefined();
    return JS_DefineProperty(cx, page, "navigation", args.get(0), JSPROP_ENUMERATE);
  }
  args.rval().setObject(*n.instance);
  return JS_WrapValue(cx, args.rval());
}
}  // namespace ruyijs::browser_environment
