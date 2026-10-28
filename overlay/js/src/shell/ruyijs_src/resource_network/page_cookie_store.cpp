// window.cookieStore: the CookieStore interface (dom/webidl/CookieStore.webidl)
// over the one cookie jar this owner already keeps. The four operations are
// page_cookie_store_methods.cpp and reach the jar through the same
// CookieAssign/CookieQuery entry points document.cookie uses; the trusted
// CookieChangeEvent the jar reports is built by the Interaction owner and is
// delivered at this object as a realm task (page_cookie_events.cpp).
#include "resource_network/page_cookie_store.h"
#include "resource_network/state.h"
#include "interaction/realm.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "runtime/web_kit.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"

namespace ruyijs::resource_network {
static const JSClass kCookieStoreDefinition = {
    "CookieStore", JSCLASS_HAS_RESERVED_SLOTS(kStoreSlotCount)};
const JSClass kCookieStoreClass = kCookieStoreDefinition;

namespace {
State& Owner(JS::HandleObject self) {
  return *static_cast<State*>(JS::GetReservedSlot(self, kStoreOwner).toPrivate());
}
bool Receiver(JSContext* cx, const JS::CallArgs& args, const char* member,
              JS::MutableHandleObject self) {
  std::string message = std::string("'") + member +
      "' called on an object that does not implement interface CookieStore.";
  return WebKit::Check(cx, args.thisv(), &kCookieStoreClass, self, message.c_str());
}
bool Listen(JSContext* cx, JS::HandleObject self, JS::HandleValue method, JS::HandleValue callback) {
  JS::RootedString type(cx, JS_NewStringCopyZ(cx, "change"));
  JS::RootedValueArray<2> arguments(cx);
  JS::RootedValue ignored(cx);
  if (!type) return false;
  arguments[0].setString(type);
  arguments[1].set(callback);
  return JS::Call(cx, self, method, arguments, &ignored);
}
// onchange is one listener on the same Interaction tree as every other handler:
// the stored handler is the listener, so it runs with the event as its argument.
bool OnChange(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args, "onchange", &self)) return false;
  Owner(self).Observe("CookieStore", "onchange", int(ApiAccess::Read));
  args.rval().set(JS::GetReservedSlot(self, kStoreHandler));
  return JS_WrapValue(cx, args.rval());
}
bool OnChangeSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args, "onchange", &self)) return false;
  Owner(self).Observe("CookieStore", "onchange", int(ApiAccess::Write));
  JS::RootedValue next(cx, args.get(0)), previous(cx, JS::GetReservedSlot(self, kStoreHandler));
  JS::RootedValue add(cx, JS::GetReservedSlot(self, kStoreAdd));
  JS::RootedValue remove(cx, JS::GetReservedSlot(self, kStoreRemove));
  if (!JS_WrapValue(cx, &next) || !JS_WrapValue(cx, &add) || !JS_WrapValue(cx, &remove))
    return false;
  if (!next.isObject() || !JS::IsCallable(&next.toObject())) next.setNull();
  if (previous.isObject() && !Listen(cx, self, remove, previous)) return false;
  if (next.isObject() && !Listen(cx, self, add, next)) return false;
  JS::SetReservedSlot(self, kStoreHandler, next);
  args.rval().setUndefined();
  return true;
}
bool Illegal(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return Error(cx, "Illegal constructor.");
}
bool WindowCookieStore(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  JSObject* self = args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject())
                                          : nullptr;
  if (self != s.page.get())
    return WebKit::Fail(cx, "'get cookieStore' called on an object that does not implement "
                            "interface Window.");
  s.Observe("Window", "cookieStore", int(ApiAccess::Read));
  args.rval().setObject(*s.cookieStore);
  return JS_WrapValue(cx, args.rval());
}
bool StoreMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  static const char* const names[] = {"get", "getAll", "set", "delete"};
  const int32_t operation = WebKit::Slot(args);
  const char* name = operation >= 0 && operation < 4 ? names[operation] : "";
  JS::RootedObject self(cx);
  if (!Receiver(cx, args, name, &self)) {
    // A promise-returning operation reports a rejected receiver as a rejection,
    // so the TypeError the brand check produced becomes the reason.
    JS::RootedValue error(cx);
    if (!JS_GetPendingException(cx, &error)) return false;
    JS_ClearPendingException(cx);
    JSObject* promise = JS::CallOriginalPromiseReject(cx, error);
    if (!promise) return false;
    args.rval().setObject(*promise);
    return true;
  }
  State& s = Owner(self);
  // Reported after receiver validation, so an invalid receiver records nothing.
  s.Observe("CookieStore", name, int(ApiAccess::Call));
  if (operation == 0) return CookieStoreGet(s, args);
  if (operation == 1) return CookieStoreGetAll(s, args);
  if (operation == 2) return CookieStoreSet(s, args);
  return CookieStoreDelete(s, args);
}
}  // namespace

JSObject* CookieItem(JSContext* cx, const CookieEntry& entry, bool withValue) {
  // One CookieListItem object: name then value, the only keys Firefox returns.
  JS::RootedObject object(cx, JS_NewPlainObject(cx));
  JS::RootedString text(cx, DecodeBodyText(cx, entry.name));
  if (!object || !text || !JS_DefineProperty(cx, object, "name", text, JSPROP_ENUMERATE))
    return nullptr;
  if (!withValue) return object;
  text = DecodeBodyText(cx, entry.value);
  if (!text || !JS_DefineProperty(cx, object, "value", text, JSPROP_ENUMERATE)) return nullptr;
  return object;
}

bool InstallCookieStore(State& s, PageHelpers& helpers) {
  JSContext* cx = s.cx;
  if (!s.events) return Error(cx, "CookieStore requires the Interaction owner");
  JS::RootedObject base(cx, s.events->prototype());
  JS::RootedValue add(cx), remove(cx);
  if (!base || !JS_GetProperty(cx, base, "addEventListener", &add) ||
      !JS_GetProperty(cx, base, "removeEventListener", &remove)) return false;
  WebFace face(cx);
  if (!s.kit.Define("CookieStore", &kCookieStoreClass, base, Illegal, &face) ||
      !s.kit.Method(face, "CookieStore", "get", StoreMethod, 0, 0) ||
      !s.kit.Method(face, "CookieStore", "getAll", StoreMethod, 0, 1) ||
      !s.kit.Method(face, "CookieStore", "set", StoreMethod, 1, 2) ||
      !s.kit.Method(face, "CookieStore", "delete", StoreMethod, 1, 3) ||
      !s.kit.Accessor(face, "CookieStore", "onchange", OnChange, OnChangeSet, 0)) return false;
  JS::RootedObject instance(cx, s.kit.New(&kCookieStoreClass, face.prototype));
  if (!instance) return false;
  JS::SetReservedSlot(instance, kStoreOwner, JS::PrivateValue(&s));
  JS::SetReservedSlot(instance, kStoreHandler, JS::NullValue());
  JS::SetReservedSlot(instance, kStoreAdd, add);
  JS::SetReservedSlot(instance, kStoreRemove, remove);
  s.cookieStore = instance;
  // The instance is an EventTarget of the Interaction tree, so the trusted change
  // event and a page's own listeners share the one dispatch path.
  if (!s.events->Attach(instance) || !helpers.Object("CookieStore", instance)) return false;
  JS::RootedObject getter(cx, s.kit.Function(WindowCookieStore, "get cookieStore", 0, 0)), setter(cx);
  return getter &&
         JS_DefineProperty(cx, s.page, "cookieStore", getter, setter, JSPROP_ENUMERATE) &&
         helpers.Accessor("Window", s.page, "cookieStore") &&
         s.secure.Add(s.page, "cookieStore") && s.secure.Add(s.page, "CookieStore");
}
}  // namespace ruyijs::resource_network
