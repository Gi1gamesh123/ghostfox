// The cross-origin WindowProxy / Location proxy handler (cross_origin.h):
// forwards while the caller is same origin with the owning window, otherwise
// HTML CrossOriginGetOwnPropertyHelper / CrossOriginGet / CrossOriginSet.
#include "browser_environment/cross_origin_internal.h"
#include "browser_environment/state.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/PropertyAndElement.h"
#include "js/PropertyDescriptor.h"
#include "js/Symbol.h"
#include <algorithm>
#include <string>
#include <vector>
namespace ruyijs::browser_environment {
namespace {
const char kFamily = 0;
// Firefox 158 CrossOriginProperties (HTML 7.2.1.3.1).
const char* const kWindowNames[] = {"window", "self", "location", "close", "closed", "focus", "blur",
                                    "frames", "length", "top", "opener", "parent", "postMessage"};
const char* const kLocationNames[] = {"href", "replace"};
struct Handler final : js::BaseProxyHandler {
  bool location;
  explicit constexpr Handler(bool isLocation) : BaseProxyHandler(&kFamily, false, true), location(isLocation) {}
  static JSObject* Target(JSObject* proxy) { return &js::GetProxyPrivate(proxy).toObject(); }
  static bool Same(JSContext* cx, JS::HandleObject proxy, bool* same) {
    if (HostCaller(cx)) return *same = true, true;
    return SameOriginWindows(cx, JS::CurrentGlobalOrNull(cx), &js::GetProxyReservedSlot(proxy, 1).toObject(), same);
  }
  static bool Named(JSContext* cx, JS::HandleId id, std::string* name) {
    name->clear();
    if (!id.isString()) return true;
    JS::RootedString string(cx, id.toString());
    JS::UniqueChars text = JS_EncodeStringToUTF8(cx, string);
    if (!text) return false;
    *name = text.get();
    return true;
  }
  bool Allowed(const std::string& name) const {
    auto match = [&](const char* n) { return name == n; };
    return location ? std::any_of(std::begin(kLocationNames), std::end(kLocationNames), match)
                    : std::any_of(std::begin(kWindowNames), std::end(kWindowNames), match);
  }
  // "then", @@toStringTag, @@hasInstance, @@isConcatSpreadable read as undefined.
  static bool Blank(JSContext* cx, JS::HandleId id, const std::string& name) {
    if (name == "then") return true;
    if (!id.isSymbol()) return false;
    for (auto code : {JS::SymbolCode::toStringTag, JS::SymbolCode::hasInstance, JS::SymbolCode::isConcatSpreadable})
      if (id == JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, code))) return true;
    return false;
  }
  static bool Deny(JSContext* cx, const std::string& name, const char* verb = "access") {
    const std::string message = "Permission denied to " + std::string(verb) + " property \"" +
                                (name.empty() ? std::string("symbol") : name) + "\" on cross-origin object";
    State* caller = WindowState(JS::CurrentGlobalOrNull(cx));
    if (caller) return ThrowDOM(*caller, "SecurityError", message.c_str());
    JS_ReportErrorASCII(cx, "%s", message.c_str());
    return false;
  }
  bool Function(JSContext* cx, JS::HandleObject proxy, const std::string& name, JS::MutableHandleValue out) const {
    JS::RootedObject cache(cx, &js::GetProxyReservedSlot(proxy, 0).toObject());
    if (!JS_GetProperty(cx, cache, name.c_str(), out)) return false;
    if (out.isObject()) return true;
    JS::RootedObject target(cx, Target(proxy));
    JS::RootedObject object(cx, NewForwarder(cx, target, name.c_str()));
    if (!object) return false;
    out.setObject(*object);
    return JS_DefineProperty(cx, cache, name.c_str(), out, 0);
  }
  bool CrossGet(JSContext* cx, JS::HandleObject proxy, const std::string& name, JS::MutableHandleValue out) const {
    JS::RootedObject target(cx, Target(proxy));
    if (location) return name == "href" ? Deny(cx, name) : Function(cx, proxy, name, out);
    if (name == "close" || name == "focus" || name == "blur" || name == "postMessage")
      return Function(cx, proxy, name, out);
    if (name == "window" || name == "self" || name == "frames") { out.setObject(*proxy); return true; }
    if (name == "location") {
      State* owner = WindowState(target);
      if (!owner) return Deny(cx, name);
      JS::RootedObject real(cx, owner->instances.get()[int32_t(Kind::Location)]);
      return LocationView(cx, owner, real, out);
    }
    if (!JS_GetProperty(cx, target, name.c_str(), out)) return false;
    if (!out.isObject()) return true;  // closed, length
    JS::RootedObject window(cx, &out.toObject());  // top, parent, opener
    return WindowView(cx, nullptr, window, out);
  }
  bool getOwnPropertyDescriptor(JSContext* cx, JS::HandleObject proxy, JS::HandleId id,
                                JS::MutableHandle<mozilla::Maybe<JS::PropertyDescriptor>> desc) const override {
    bool same = false;
    JS::RootedObject target(cx, Target(proxy));
    if (!Same(cx, proxy, &same)) return false;
    if (same) return JS_GetOwnPropertyDescriptorById(cx, target, id, desc);
    std::string name;
    JS::RootedValue value(cx);
    if (!Named(cx, id, &name)) return false;
    if (!Blank(cx, id, name)) {
      if (!Allowed(name)) return Deny(cx, name);
      if (!(location && name == "href") && !CrossGet(cx, proxy, name, &value)) return false;
    }
    desc.set(mozilla::Some(JS::PropertyDescriptor::Data(value, {JS::PropertyAttribute::Configurable})));
    return true;
  }
  bool defineProperty(JSContext* cx, JS::HandleObject proxy, JS::HandleId id, JS::Handle<JS::PropertyDescriptor> desc,
                      JS::ObjectOpResult& result) const override {
    bool same = false;
    JS::RootedObject target(cx, Target(proxy));
    std::string name;
    if (!Same(cx, proxy, &same)) return false;
    if (same) return JS_DefinePropertyById(cx, target, id, desc, result);
    return Named(cx, id, &name) && Deny(cx, name, "define");
  }
  bool ownPropertyKeys(JSContext* cx, JS::HandleObject proxy, JS::MutableHandleIdVector props) const override {
    bool same = false;
    JS::RootedObject target(cx, Target(proxy));
    if (!Same(cx, proxy, &same)) return false;
    if (same) return js::GetPropertyKeys(cx, target, JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS, props);
    std::vector<const char*> names = location
        ? std::vector<const char*>(std::begin(kLocationNames), std::end(kLocationNames))
        : std::vector<const char*>(std::begin(kWindowNames), std::end(kWindowNames));
    names.push_back("then");
    for (const char* name : names) {
      JS::RootedString atom(cx, JS_AtomizeAndPinString(cx, name));
      if (!atom || !props.append(JS::PropertyKey::fromPinnedString(atom))) return false;
    }
    return true;
  }
  bool delete_(JSContext* cx, JS::HandleObject proxy, JS::HandleId id, JS::ObjectOpResult& result) const override {
    bool same = false;
    JS::RootedObject target(cx, Target(proxy));
    std::string name;
    if (!Same(cx, proxy, &same)) return false;
    if (same) return JS_DeletePropertyById(cx, target, id, result);
    return Named(cx, id, &name) && Deny(cx, name, "delete");
  }
  bool getPrototypeIfOrdinary(JSContext*, JS::HandleObject, bool* ordinary, JS::MutableHandleObject) const override {
    return *ordinary = false, true;
  }
  bool getPrototype(JSContext* cx, JS::HandleObject proxy, JS::MutableHandleObject protop) const override {
    bool same = false;
    JS::RootedObject target(cx, Target(proxy));
    if (!Same(cx, proxy, &same)) return false;
    if (same) return JS_GetPrototype(cx, target, protop);
    protop.set(nullptr);
    return true;
  }
  // [[SetPrototypeOf]] of WindowProxy and Location is SetImmutablePrototype.
  bool setPrototype(JSContext* cx, JS::HandleObject proxy, JS::HandleObject proto,
                    JS::ObjectOpResult& result) const override {
    JS::RootedObject current(cx);
    if (!getPrototype(cx, proxy, &current)) return false;
    return current == proto ? result.succeed() : result.failCantSetProto();
  }
  bool preventExtensions(JSContext*, JS::HandleObject, JS::ObjectOpResult& r) const override { return r.failCantPreventExtensions(); }
  bool isExtensible(JSContext*, JS::HandleObject, bool* extensible) const override { return *extensible = true, true; }
  bool has(JSContext* cx, JS::HandleObject proxy, JS::HandleId id, bool* bp) const override {
    bool same = false;
    JS::RootedObject target(cx, Target(proxy));
    std::string name;
    if (!Same(cx, proxy, &same)) return false;
    if (same) return JS_HasPropertyById(cx, target, id, bp);
    if (!Named(cx, id, &name)) return false;
    if (!Blank(cx, id, name) && !Allowed(name)) return Deny(cx, name);
    *bp = true;
    return true;
  }
  bool get(JSContext* cx, JS::HandleObject proxy, JS::HandleValue receiver, JS::HandleId id,
           JS::MutableHandleValue vp) const override {
    bool same = false;
    JS::RootedObject target(cx, Target(proxy));
    std::string name;
    if (!Same(cx, proxy, &same)) return false;
    if (same) {
      JS::RootedValue self(cx, receiver.isObject() && &receiver.toObject() == proxy ? JS::ObjectValue(*target) : receiver);
      return JS_ForwardGetPropertyTo(cx, target, id, self, vp);
    }
    if (!Named(cx, id, &name)) return false;
    if (Blank(cx, id, name)) { vp.setUndefined(); return true; }
    return Allowed(name) ? CrossGet(cx, proxy, name, vp) : Deny(cx, name);
  }
  bool set(JSContext* cx, JS::HandleObject proxy, JS::HandleId id, JS::HandleValue v, JS::HandleValue receiver,
           JS::ObjectOpResult& result) const override {
    bool same = false;
    JS::RootedObject target(cx, Target(proxy));
    std::string name;
    if (!Same(cx, proxy, &same)) return false;
    if (same) {
      JS::RootedValue self(cx, receiver.isObject() && &receiver.toObject() == proxy ? JS::ObjectValue(*target) : receiver);
      return JS_ForwardSetPropertyTo(cx, target, id, v, self, result);
    }
    if (!Named(cx, id, &name)) return false;
    // The two cross-origin writes: window.location and location.href (navigation).
    if ((location && name == "href") || (!location && name == "location"))
      return JS_SetProperty(cx, target, name.c_str(), v) && result.succeed();
    return Deny(cx, name, "set");
  }
  const char* className(JSContext*, JS::HandleObject) const override { return "Object"; }
};
const Handler kWindowHandler(false), kLocationHandler(true);
}  // namespace
const js::BaseProxyHandler* CrossOriginWindowHandler() { return &kWindowHandler; }
const js::BaseProxyHandler* CrossOriginLocationHandler() { return &kLocationHandler; }
}  // namespace ruyijs::browser_environment
