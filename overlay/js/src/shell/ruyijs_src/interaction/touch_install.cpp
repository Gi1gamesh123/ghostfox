#include "interaction/touch.h"
#include "runtime/page_helpers.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Symbol.h"

namespace ruyijs::interaction {
namespace {
bool Illegal(JSContext* cx, unsigned, JS::Value*) {
  return Error(cx, "TypeError", "Illegal constructor.");
}
bool Install(State& state, const char* name, JSNative callback, unsigned length,
             const JSPropertySpec* properties, const JSFunctionSpec* methods,
             JS::MutableHandleObject result) {
  JSContext* cx = state.cx;
  result.set(JS_NewPlainObject(cx));
  JS::RootedFunction function(cx, JS_NewFunction(cx, callback, length, JSFUN_CONSTRUCTOR, name));
  if (!result || !function) return false;
  JS::RootedObject constructor(cx, JS_GetFunctionObject(function));
  JS::RootedValue metadata(cx);
  if (!JS_GetProperty(cx, constructor, "length", &metadata) ||
      !JS_GetProperty(cx, constructor, "name", &metadata) ||
      !JS_DefineProperty(cx, constructor, "prototype", result,
                         JSPROP_READONLY | JSPROP_PERMANENT) ||
      (methods && !JS_DefineFunctions(cx, result, methods)) ||
      !JS_DefineProperties(cx, result, properties) ||
      !JS_DefineProperty(cx, result, "constructor", constructor, 0)) return false;
  if (methods) {
    JS::RootedObject array(cx, JS::GetRealmArrayPrototype(cx));
    JS::RootedValue iterator(cx);
    JS::RootedId id(cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::iterator)));
    if (!array || !JS_GetProperty(cx, array, "values", &iterator) ||
        !JS_DefinePropertyById(cx, result, id, iterator, 0)) return false;
  }
  if (!Tag(cx, result, name) || !JS_DefineProperty(cx, state.page, name, constructor, 0) ||
      !state.helpers->Interface(name, result) || !state.helpers->Constructor(name, constructor)) return false;
  for (const JSPropertySpec* p = properties; p->name.string(); ++p)
    if (!state.helpers->Accessor(name, result, p->name.string())) return false;
  for (const JSFunctionSpec* m = methods; m && m->name.string(); ++m) {
    JS::RootedValue value(cx);
    if (!JS_GetProperty(cx, result, m->name.string(), &value)) return false;
    JS::RootedObject method(cx, &value.toObject());
    if (!state.helpers->Member(method, name, m->name.string(), "call")) return false;
  }
  return true;
}
}
bool InstallTouchInterfaces(State& state) {
  JS::RootedObject touch(state.cx), list(state.cx);
  if (!Install(state, "Touch", TouchValueConstructor, 1, kTouchValueProperties, nullptr, &touch) ||
      !Install(state, "TouchList", Illegal, 0, kTouchListProperties, kTouchListMethods, &list)) return false;
  state.touchPrototype = touch;
  state.touchListPrototype = list;
  return true;
}
}
