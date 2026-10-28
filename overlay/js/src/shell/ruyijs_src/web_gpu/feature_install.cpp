#include "web_gpu/internal.h"
#include "js/Realm.h"
#include "js/Symbol.h"

namespace ruyijs::web_gpu {
namespace {
bool Interface(State& state, WebFace& face, const char* name, const JSClass* clasp, int base) {
  JSContext* cx = state.cx;
  JS::RootedObject plain(cx, JS::GetRealmObjectPrototype(cx));
  if (!state.kit.Define(name, clasp, plain, IllegalCreate, &face)) return false;
  struct Member { const char* name; unsigned length; int id; };
  constexpr Member methods[] = {{"entries",0,1},{"keys",0,2},{"values",0,3},
                                 {"forEach",1,4},{"has",1,0}};
  for (const auto& method : methods)
    if (!state.kit.Method(face, name, method.name, FeaturesMethod, method.length, base+method.id))
      return false;
  if (!state.kit.Accessor(face, name, "size", FeaturesProperty, nullptr, base) ||
      !JS_DeleteProperty(cx, face.prototype, "constructor") ||
      !JS_DefineProperty(cx, face.prototype, "constructor", face.constructor, 0)) return false;
  JS::RootedId iterator(cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::iterator)));
  JS::RootedValue values(cx);
  return JS_GetProperty(cx, face.prototype, "values", &values) &&
         JS_DefinePropertyById(cx, face.prototype, iterator, values, 0);
}
}
bool InstallFeatureInterfaces(State& state) {
  return Interface(state, state.features, "GPUSupportedFeatures", &kFeaturesClass, 0) &&
         Interface(state, state.wgsl_features, "WGSLLanguageFeatures", &kWgslFeaturesClass, kWgslMemberBase) &&
         state.kit.Accessor(state.gpu, "GPU", "wgslLanguageFeatures", WgslFeaturesProperty, nullptr, 0);
}
}
