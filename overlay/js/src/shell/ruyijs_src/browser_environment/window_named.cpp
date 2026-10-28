// Window's inherited names read the current Document through one port; this
// object owns no DOM index or copied properties. See WINDOW_NAMED_PROPERTIES.md.
#include "browser_environment/window_named.h"
#include "browser_environment/state.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/PropertyDescriptor.h"
#include "js/Proxy.h"
#include "js/String.h"
#include "js/Symbol.h"

namespace ruyijs::browser_environment {
namespace {
char kNamedFamily;
const JSClass kNamedClass = PROXY_CLASS_DEF(
    "WindowProperties", JSCLASS_HAS_RESERVED_SLOTS(1));

class Handler final : public js::BaseProxyHandler {
 public:
  constexpr Handler() : BaseProxyHandler(&kNamedFamily, true) {}
  bool getOwnPropertyDescriptor(JSContext* cx, JS::HandleObject proxy, JS::HandleId id,
      JS::MutableHandle<mozilla::Maybe<JS::PropertyDescriptor>> desc) const override {
    desc.reset();
    if (id.isSymbol()) {
      if (!id.isWellKnownSymbol(JS::SymbolCode::toStringTag)) return true;
      JS::RootedString tag(cx, JS_NewStringCopyZ(cx, "WindowProperties"));
      if (!tag) return false;
      desc.set(mozilla::Some(JS::PropertyDescriptor::Data(JS::StringValue(tag),
          {JS::PropertyAttribute::Configurable})));
      return true;
    }
    JS::RootedObject prototype(cx);
    bool inherited = false;
    if (!JS_GetPrototype(cx, proxy, &prototype) ||
        (prototype && !JS_HasPropertyById(cx, prototype, id, &inherited))) return false;
    if (inherited) return true;
    auto* state = static_cast<State*>(js::GetProxyReservedSlot(proxy, 0).toPrivate());
    if (!state->windowNamePort) return true;
    JS::RootedValue key(cx), value(cx);
    if (!JS_IdToValue(cx, id, &key)) return false;
    JS::RootedString name(cx, JS::ToString(cx, key));
    if (!name) return false;
    if (!JS_GetStringLength(name)) return true;
    if (!state->windowNamePort(state->windowNameContext, name, &value)) return false;
    if (value.isUndefined()) return true;
    desc.set(mozilla::Some(JS::PropertyDescriptor::Data(value,
        {JS::PropertyAttribute::Configurable, JS::PropertyAttribute::Writable})));
    return true;
  }
  bool defineProperty(JSContext*, JS::HandleObject, JS::HandleId,
      JS::Handle<JS::PropertyDescriptor>, JS::ObjectOpResult& result) const override {
    return result.failCantDefineWindowNamedProperty();
  }
  bool ownPropertyKeys(JSContext* cx, JS::HandleObject,
                       JS::MutableHandleIdVector keys) const override {
    return keys.append(JS::GetWellKnownSymbolKey(cx, JS::SymbolCode::toStringTag));
  }
  bool delete_(JSContext*, JS::HandleObject, JS::HandleId,
               JS::ObjectOpResult& result) const override {
    return result.failCantDeleteWindowNamedProperty();
  }
  bool getPrototypeIfOrdinary(JSContext* cx, JS::HandleObject proxy, bool* ordinary,
                              JS::MutableHandleObject prototype) const override {
    *ordinary = true;
    return JS_GetPrototype(cx, proxy, prototype);
  }
  bool preventExtensions(JSContext*, JS::HandleObject,
                          JS::ObjectOpResult& result) const override {
    return result.failCantPreventExtensions();
  }
  bool isExtensible(JSContext*, JS::HandleObject, bool* extensible) const override {
    *extensible = true;
    return true;
  }
  const char* className(JSContext*, JS::HandleObject) const override {
    return "WindowProperties";
  }
};
const Handler handler;
}

JSObject* NewWindowNamedProperties(State& state, JS::HandleObject prototype) {
  js::ProxyOptions options;
  options.setClass(&kNamedClass);
  JS::RootedObject object(state.cx, js::NewProxyObject(state.cx, &handler,
      JS::NullHandleValue, prototype, options));
  if (!object) return nullptr;
  js::SetProxyReservedSlot(object, 0, JS::PrivateValue(&state));
  bool immutable = false;
  if (!JS_SetImmutablePrototype(state.cx, object, &immutable) || !immutable) return nullptr;
  return object;
}
}
