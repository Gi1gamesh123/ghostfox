// The FormData installer. It defines the constructor, the entry operations and the
// whole iterator protocol (entries, keys, values, @@iterator and the shared next
// method) on the installing realm, and registers every member with the donor trace
// registry.
#include "web_platform/state.h"

#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Symbol.h"

namespace ruyijs::web_platform {
namespace {
struct MethodDef {
  const char* name;
  unsigned length;
  int32_t slot;
};

constexpr MethodDef kMethods[] = {
    {"append", 2, kFormDataAppend}, {"delete", 1, kFormDataDelete},
    {"get", 1, kFormDataGet},       {"getAll", 1, kFormDataGetAll},
    {"has", 1, kFormDataHas},       {"set", 2, kFormDataSet},
    {"forEach", 1, kFormDataForEach}};
}  // namespace

bool InstallFormData(State& state, PageHelpers& helpers) {
  JSContext* cx = state.cx;
  WebKit& kit = state.kit;
  JS::RootedObject objectProto(cx, JS::GetRealmObjectPrototype(cx));
  JS::RootedId iteratorId(
      cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::iterator)));
  if (!objectProto ||
      !kit.Define("FormData", &kFormDataClass, objectProto, FormDataConstructor,
                  &state.formData))
    return false;
  for (const MethodDef& method : kMethods)
    if (!kit.Method(state.formData, "FormData", method.name, FormDataMethod,
                    method.length, method.slot))
      return false;
  JS::RootedObject iteratorProto(cx, JS_NewPlainObject(cx));
  JS::RootedObject entries(cx, kit.Function(FormDataIterator, "entries", 0, kFormDataEntries));
  JS::RootedObject keys(cx, kit.Function(FormDataIterator, "keys", 0, kFormDataKeys));
  JS::RootedObject values(cx, kit.Function(FormDataIterator, "values", 0, kFormDataValues));
  JS::RootedObject next(cx, kit.Function(FormDataNext, "next", 0, 0));
  JS::RootedObject identity(cx, kit.Function(FormDataIdentity, "[Symbol.iterator]", 0, 0));
  if (!iteratorProto || !entries || !keys || !values || !next || !identity ||
      !JS_SetPrototype(cx, iteratorProto, objectProto) ||
      !JS_DefineProperty(cx, iteratorProto, "next", next, JSPROP_ENUMERATE) ||
      !JS_DefinePropertyById(cx, iteratorProto, iteratorId, identity, JSPROP_READONLY) ||
      !JS_DefineProperty(cx, state.formData.prototype, "entries", entries, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, state.formData.prototype, "keys", keys, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, state.formData.prototype, "values", values, JSPROP_ENUMERATE) ||
      !JS_DefinePropertyById(cx, state.formData.prototype, iteratorId, entries, JSPROP_READONLY))
    return false;
  state.formDataIteratorPrototype = iteratorProto.get();
  return helpers.Member(entries, "FormData", "entries", "call") &&
         helpers.Member(keys, "FormData", "keys", "call") &&
         helpers.Member(values, "FormData", "values", "call") &&
         helpers.Member(next, "FormData Iterator", "next", "call");
}
}  // namespace ruyijs::web_platform
