#include "browser_environment/browser_chrome.h"
#include "runtime/page_helpers.h"
#include "runtime/native_constructor_trace.h"
#include "js/Object.h"
#include "js/Symbol.h"
#include <iterator>

namespace ruyijs::browser_environment {
const JSClass kWindowFacadeClass = {"WindowPrototype", JSCLASS_HAS_RESERVED_SLOTS(kWindowFacadeSlots)};
const JSClass kBarPropClass = {"BarProp", JSCLASS_HAS_RESERVED_SLOTS(1)};
const JSClass kUserActivationClass = {"UserActivation", JSCLASS_HAS_RESERVED_SLOTS(1)};
namespace {
bool Illegal(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return Error(cx, "Illegal constructor");
}
bool Member(State& s, JS::HandleObject prototype, const char* iface,
            const char* name, int32_t slot, JSNative getter, JSNative setter = nullptr) {
  return s.Accessor(prototype, name, slot, JSPROP_ENUMERATE, getter, setter) &&
         s.helpers->Accessor(iface, prototype, name);
}
bool Interface(State& s, const JSClass& shape, JS::HandleObject prototype) {
  JSContext* cx = s.cx;
  JS::RootedObject constructor(cx, s.Function(Illegal, shape.name, 0, 0, JSFUN_CONSTRUCTOR));
  JS::RootedString tag(cx, JS_NewStringCopyZ(cx, shape.name));
  JS::RootedId key(cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::toStringTag)));
  return constructor && tag &&
      JS_DefineProperty(cx, constructor, "prototype", prototype, JSPROP_READONLY | JSPROP_PERMANENT) &&
      JS_DefineProperty(cx, prototype, "constructor", constructor, 0) &&
      JS_DefinePropertyById(cx, prototype, key, tag, JSPROP_READONLY) &&
      JS_DefineProperty(cx, s.page, shape.name, constructor, 0) &&
      s.helpers->Interface(shape.name, prototype) && s.helpers->Object(shape.name, prototype) &&
      s.helpers->Constructor(shape.name, constructor);
}
JSObject* Instance(State& s, const JSClass* shape, JS::HandleObject prototype) {
  JS::RootedObject object(s.cx, JS_NewObjectWithGivenProto(s.cx, shape, prototype));
  if (object) JS::SetReservedSlot(object, 0, JS::PrivateValue(&s));
  return object;
}
}
bool InstallBrowserChrome(State& s) {
  JSContext* cx = s.cx;
  JS::RootedObject bars(cx, JS_NewPlainObject(cx));
  JS::RootedObject activation(cx, JS_NewPlainObject(cx));
  if (!bars || !activation ||
      !Member(s, bars, "BarProp", "visible", 0, ChromeValueGet, ChromeVisibleSet) ||
      !Member(s, activation, "UserActivation", "hasBeenActive", 1, ChromeValueGet) ||
      !Member(s, activation, "UserActivation", "isActive", 2, ChromeValueGet) ||
      !Interface(s, kBarPropClass, bars) || !Interface(s, kUserActivationClass, activation)) return false;
  JS::RootedObject windowPrototype(cx);
  if (!JS_GetPrototype(cx, s.page, &windowPrototype)) return false;
  for (unsigned i = 0; i < std::size(kBarNames); ++i) {
    JS::RootedObject object(cx, Instance(s, &kBarPropClass, bars));
    if (!object || !Member(s, s.page, "Window", kBarNames[i], int32_t(i),
                            ChromeBarGet, ChromeBarSet)) return false;
    JS::SetReservedSlot(windowPrototype, i + 1, JS::ObjectValue(*object));
  }
  JS::RootedObject object(cx, Instance(s, &kUserActivationClass, activation));
  JS::RootedObject navigator(cx, s.instances.get()[int32_t(Kind::Navigator)]);
  if (!object || !Member(s, s.navigatorPrototype, "Navigator", "userActivation", 0,
                          ChromeActivationGet)) return false;
  JS::SetReservedSlot(navigator, kNavigatorActivationSlot, JS::ObjectValue(*object));
  return true;
}
}
