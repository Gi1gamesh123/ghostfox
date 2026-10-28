// Cross-origin WindowProxy / Location views (cross_origin.h): the window
// registry, the per-access same-origin check and the one view per window.
#include "browser_environment/cross_origin_internal.h"
#include "browser_environment/state.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/PropertyAndElement.h"
#include "js/ScriptPrivate.h"
#include <algorithm>
#include <cstring>
#include <vector>

namespace ruyijs::browser_environment {
namespace {
std::vector<State*>& Windows() { static std::vector<State*> list; return list; }
bool Forward(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject callee(cx, &args.callee());
  JS::RootedValue target(cx, js::GetFunctionNativeReserved(callee, 0));
  JS::RootedString name(cx, js::GetFunctionNativeReserved(callee, 1).toString());
  JS::RootedObject object(cx, &target.toObject());
  JS::RootedValue method(cx);
  JS::UniqueChars text = JS_EncodeStringToUTF8(cx, name);
  if (!text || !JS_GetProperty(cx, object, text.get(), &method)) return false;
  JS::RootedValueVector values(cx);
  for (unsigned i = 0; i < args.length(); ++i)
    if (!values.append(args[i])) return false;
  return JS::Call(cx, target, method, JS::HandleValueArray(values), args.rval());
}
JSObject* NewView(JSContext* cx, const js::BaseProxyHandler* handler, JS::HandleObject real, JSObject* owner) {
  JS::RootedValue priv(cx, JS::ObjectValue(*real));
  JS::RootedObject cache(cx, JS_NewObjectWithGivenProto(cx, nullptr, nullptr));
  JS::RootedObject proxy(cx, cache ? js::NewProxyObject(cx, handler, priv, nullptr) : nullptr);
  if (!proxy) return nullptr;
  js::SetProxyReservedSlot(proxy, 0, JS::ObjectValue(*cache));
  js::SetProxyReservedSlot(proxy, 1, JS::ObjectValue(*owner));
  return proxy;
}
}  // namespace

JSObject* NewForwarder(JSContext* cx, JS::HandleObject target, const char* name) {
  JSFunction* fn = js::NewFunctionWithReserved(cx, Forward, std::strcmp(name, "postMessage") ? 0 : 1, 0, name);
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, name));
  if (!fn || !text) return nullptr;
  JS::RootedObject object(cx, JS_GetFunctionObject(fn));
  js::SetFunctionNativeReserved(object, 0, JS::ObjectValue(*target));
  js::SetFunctionNativeReserved(object, 1, JS::StringValue(text));
  return object;
}
bool LocationView(JSContext* cx, State* owner, JS::HandleObject real, JS::MutableHandleValue out) {
  if (!owner->crossLocation) {
    JS::RootedObject window(cx, owner->page);
    JSObject* view = NewView(cx, CrossOriginLocationHandler(), real, window);
    if (!view) return false;
    owner->crossLocation = std::make_unique<JS::PersistentRootedObject>(cx, view);
  }
  out.setObject(*owner->crossLocation->get());
  return true;
}
// Script privates are otherwise used only by module records (a URL string).
constexpr int32_t kHostScript = 0x52594a53;
void MarkHostScript(JSScript* script) { JS::SetScriptPrivate(script, JS::Int32Value(kHostScript)); }
bool HostCaller(JSContext* cx) {
  JS::Value value = JS::GetScriptedCallerPrivate(cx);
  return value.isInt32() && value.toInt32() == kHostScript;
}
void RegisterWindow(State* state) { Windows().push_back(state); }
void UnregisterWindow(State* state) {
  auto& list = Windows();
  list.erase(std::remove(list.begin(), list.end(), state), list.end());
}
State* WindowState(JSObject* global) {
  for (State* state : Windows())
    if (global && !state->worker && state->page.get() == global) return state;
  return nullptr;
}
bool SameOriginWindows(JSContext* cx, JSObject* viewer, JSObject* target, bool* same) {
  State* a = WindowState(viewer);
  State* b = WindowState(target);
  *same = true;
  if (!a || !b || a == b) return true;
  OriginValue x, y;
  {
    JSAutoRealm realm(cx, a->page);
    if (!a->ReadOriginValue(&x)) return false;
  }
  {
    JSAutoRealm realm(cx, b->page);
    if (!b->ReadOriginValue(&y)) return false;
  }
  using ruyijs::domain::browser_environment::OriginKind;
  *same = x.kind == OriginKind::Tuple && y.kind == OriginKind::Tuple && x.tuple == y.tuple;
  return true;
}
bool WindowView(JSContext* cx, JSObject* viewer, JS::HandleObject target, JS::MutableHandleValue out) {
  bool same = true;
  if (!SameOriginWindows(cx, viewer ? viewer : JS::CurrentGlobalOrNull(cx), target, &same)) return false;
  State* owner = WindowState(target);
  if (same || !owner) {
    out.setObject(*target);
    return JS_WrapValue(cx, out);
  }
  if (!owner->crossWindow) {
    JSObject* view = NewView(cx, CrossOriginWindowHandler(), target, target);
    if (!view) return false;
    owner->crossWindow = std::make_unique<JS::PersistentRootedObject>(cx, view);
  }
  out.setObject(*owner->crossWindow->get());
  return true;
}
}  // namespace ruyijs::browser_environment
