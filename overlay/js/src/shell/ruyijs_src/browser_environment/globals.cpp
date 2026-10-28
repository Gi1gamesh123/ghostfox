#include "runtime/web_kit.h"
#include "browser_environment/state.h"
#include "browser_environment/cross_origin.h"
#include "browser_environment/browser_chrome.h"
#include "browser_environment/window_named.h"
#include "runtime/page_helpers.h"
#include "jsfriendapi.h"
#include "js/PropertyAndElement.h"
#include "js/Object.h"
#include "js/Symbol.h"

namespace ruyijs::browser_environment {
static constexpr const char* kNames[] = {
  "window", "self", "top", "parent", "navigator", "screen", "location", "history"
};
// Install-time member name of one global accessor, including the frameElement
// slot that is not part of kNames.
static const char* GlobalName(int32_t slot) {
  return slot < 8 ? kNames[slot] : "frameElement";
}
static State* GlobalReceiver(JSContext* cx, const JS::CallArgs& args) {
  auto* state = static_cast<State*>(
      js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  if (!state) {
    WebKit::FailBrand(cx, args, "Window");
    return nullptr;
  }
  // The Window named properties are WebIDL accessors of the global prototype
  // chain here; the donor's MaybeGlobalThisPolicy resolves a bare call
  // (undefined) and an explicit null to the callee realm's global
  // (dom/bindings/BindingUtils.cpp:2863-2874). Any other receiver is rejected.
  bool is_page = args.thisv().isNullOrUndefined() ||
                 (args.thisv().isObject() && &args.thisv().toObject() == state->page);
  if (!is_page) {
    WebKit::FailBrand(cx, args, "Window");
    return nullptr;
  }
  return state;
}
static bool GlobalGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  State* state = GlobalReceiver(cx, args);
  if (!state) return false;
  int32_t slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  state->Observe("Window", GlobalName(slot), int(ApiAccess::Read));
  if (slot == 8) {
    // frameElement is null unless the embedder's document is same origin.
    bool same = true;
    JS::RootedObject parent(cx, state->parentWindow);
    if (state->frameElement && !SameOriginWindows(cx, state->page, parent, &same)) return false;
    args.rval().setObjectOrNull(same ? state->frameElement.get() : nullptr);
    return JS_WrapValue(cx, args.rval());
  }
  if (slot == 2 || slot == 3) {  // top / parent, seen from this window
    JS::RootedObject window(cx, slot == 2 ? state->topWindow : state->parentWindow);
    return WindowView(cx, state->page, window, args.rval());
  }
  args.rval().setObject(slot < 4 ? *state->page : *state->instances.get()[slot - 4]);
  return JS_WrapValue(cx, args.rval());
}
static bool Replace(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  State* state = GlobalReceiver(cx, args);
  if (!state) return false;
  int32_t slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  state->Observe("Window", GlobalName(slot), int(ApiAccess::Write));
  JS::RootedValue value(cx, argc ? args[0] : JS::UndefinedValue());
  if (!JS_DefineProperty(cx, state->page, kNames[slot], value, JSPROP_ENUMERATE)) return false;
  args.rval().setUndefined();
  return true;
}
// Window.location is [PutForwards=href], not a replaceable Window property.
// Forward to this realm's one Location setter, including its receiver checks
// and the navigation port used by document.location and location.href.
static bool LocationSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  State* state = GlobalReceiver(cx, args);
  if (!state) return false;
  state->Observe("Window", "location", int(ApiAccess::Write));
  JS::RootedObject location(cx, state->instances.get()[int32_t(Kind::Location)]);
  JS::RootedValue value(cx, args.get(0));
  args.rval().setUndefined();
  return JS_SetProperty(cx, location, "href", value);
}
// window.opener: the donor's GetOpener returns null when the browsing context
// has no opener, and its SetOpener defines the value on the window object
// itself (dom/base/nsGlobalWindowInner.cpp:3659-3690). A null assignment keeps
// the accessor; any other value replaces it, so it resets on navigation.
static bool OpenerGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  State* state = GlobalReceiver(cx, args);
  if (!state) return false;
  state->Observe("Window", "opener", int(ApiAccess::Read));
  args.rval().setNull();
  return true;
}
static bool OpenerSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  State* state = GlobalReceiver(cx, args);
  if (!state) return false;
  state->Observe("Window", "opener", int(ApiAccess::Write));
  JS::RootedValue value(cx, argc ? args[0] : JS::UndefinedValue());
  args.rval().setUndefined();
  if (value.isNull()) return true;
  return JS_DefineProperty(cx, state->page, "opener", value, JSPROP_ENUMERATE);
}
bool State::Globals() {
  for (int32_t index = 0; index < 8; ++index) {
    bool permanent = index == 0 || index == 2 || index == 6;
    bool replaceable = index == 1 || index == 3 || index == 5;
    if (!Accessor(page, kNames[index], index,
                    JSPROP_ENUMERATE | (permanent ? JSPROP_PERMANENT : 0),
                    GlobalGet, index == 6 ? LocationSet : replaceable ? Replace : nullptr) ||
        !helpers->Accessor("Window", page, kNames[index])) return false;
  }
  return Accessor(page, "frameElement", 8, JSPROP_ENUMERATE, GlobalGet) &&
         helpers->Accessor("Window", page, "frameElement") && helpers->Object("Window", page) &&
         // window.opener is an own accessor of the global, never a prototype
         // member; the getter returns null because ruyijs opens no page in a
         // child context (tests/fixtures/window_opener_firefox.json).
         Accessor(page, "opener", 0, JSPROP_ENUMERATE, OpenerGet, OpenerSet) &&
         helpers->Accessor("Window", page, "opener");
}
static bool IllegalWindow(JSContext* cx, unsigned, JS::Value*) {
  return Error(cx, "Illegal constructor");
}
static bool Tag(JSContext* cx, JS::HandleObject object, const char* value) {
  JS::RootedId id(cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::toStringTag)));
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, value));
  return text && JS_DefinePropertyById(cx, object, id, text, JSPROP_READONLY);
}
// Window is [Global]: its members stay on the global itself. The chain is
// window -> Window.prototype -> WindowProperties -> EventTarget.prototype, and
// the global's prototype is immutable as in Firefox.
bool State::Window(JS::HandleObject eventPrototype) {
  JS::RootedObject properties(cx, NewWindowNamedProperties(*this, eventPrototype));
  JS::RootedObject prototype(cx, properties ? JS_NewObjectWithGivenProto(cx, &kWindowFacadeClass, properties) : nullptr);
  JSFunction* function = prototype ? JS_NewFunction(cx, IllegalWindow, 0, JSFUN_CONSTRUCTOR, "Window") : nullptr;
  if (!function) return false;
  JS::SetReservedSlot(prototype, 0, JS::PrivateValue(this));
  JS::RootedObject constructor(cx, JS_GetFunctionObject(function));
  JS::RootedValue parent(cx);
  bool immutable = false;
  if (!JS_GetProperty(cx, eventPrototype, "constructor", &parent) || !parent.isObject()) return false;
  JS::RootedObject parentConstructor(cx, &parent.toObject());
  return JS_SetPrototype(cx, constructor, parentConstructor) &&
         JS_DefineProperty(cx, constructor, "prototype", prototype, JSPROP_READONLY | JSPROP_PERMANENT) &&
         JS_DefineProperty(cx, prototype, "constructor", constructor, 0) &&
         Tag(cx, prototype, "Window") &&
         JS_DefineProperty(cx, page, "Window", constructor, 0) &&
         JS_SetPrototype(cx, page, prototype) && JS_SetImmutablePrototype(cx, page, &immutable) &&
         immutable && helpers->Interface("Window", prototype) &&
         helpers->Constructor("Window", constructor);
}
}
