// The URLSearchParams installer. It defines the constructor, the query
// operations, the size accessor and the whole iterator protocol (entries, keys,
// values, @@iterator and the shared next method) on the installing realm, and
// registers every function with the donor trace registry.
#include "web_platform/state.h"

#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/Realm.h"
#include "js/Id.h"
#include "js/PropertyAndElement.h"
#include "js/Symbol.h"

namespace ruyijs::web_platform {
namespace {
// The iterator kind carried by the slots handed to ParamsIterator; it mirrors
// the kind values the iterator behaviour file reads back.
enum IteratorKind { kEntries = 0, kKeys, kValues };

struct MethodDef {
  const char* name;
  int32_t slot;
  unsigned length;
};

constexpr MethodDef kMethods[] = {
    {"append", kAppend, 2},   {"delete", kDelete, 1},
    {"get", kGet, 1},         {"getAll", kGetAll, 1},
    {"has", kHas, 1},         {"set", kSet, 2},
    {"sort", kSort, 0},       {"toString", kToString, 0},
    {"forEach", kForEach, 1}};
}  // namespace

bool InstallSearchParams(State& state, PageHelpers& helpers) {
  JSContext* cx = state.cx;
  WebKit& kit = state.kit;
  JS::RootedObject objectProto(cx, JS::GetRealmObjectPrototype(cx));
  JS::RootedId iteratorId(
      cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::iterator)));
  if (!objectProto ||
      !kit.Define("URLSearchParams", &kSearchParamsClass, objectProto,
                  ParamsConstructor, &state.params))
    return false;
  for (const MethodDef& method : kMethods)
    if (!kit.Method(state.params, "URLSearchParams", method.name, ParamsMethod,
                    method.length, method.slot))
      return false;
  if (!kit.Accessor(state.params, "URLSearchParams", "size", ParamsGetter, nullptr, 0))
    return false;
  JS::RootedObject iteratorProto(cx, JS_NewPlainObject(cx));
  JS::RootedObject entries(cx, kit.Function(ParamsIterator, "entries", 0, kEntries));
  JS::RootedObject keys(cx, kit.Function(ParamsIterator, "keys", 0, kKeys));
  JS::RootedObject values(cx, kit.Function(ParamsIterator, "values", 0, kValues));
  JS::RootedObject next(cx, kit.Function(ParamsNext, "next", 0, 0));
  JS::RootedObject identity(cx, kit.Function(ParamsIdentity, "[Symbol.iterator]", 0, 0));
  if (!iteratorProto || !entries || !keys || !values || !next || !identity ||
      !JS_SetPrototype(cx, iteratorProto, objectProto) ||
      !JS_DefineProperty(cx, iteratorProto, "next", next, JSPROP_ENUMERATE) ||
      !JS_DefinePropertyById(cx, iteratorProto, iteratorId, identity, JSPROP_READONLY) ||
      !JS_DefineProperty(cx, state.params.prototype, "entries", entries, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, state.params.prototype, "keys", keys, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, state.params.prototype, "values", values, JSPROP_ENUMERATE) ||
      !JS_DefinePropertyById(cx, state.params.prototype, iteratorId, entries,
                             JSPROP_READONLY))
    return false;
  state.paramsIteratorPrototype = iteratorProto.get();
  return helpers.Member(entries, "URLSearchParams", "entries", "call") &&
         helpers.Member(keys, "URLSearchParams", "keys", "call") &&
         helpers.Member(values, "URLSearchParams", "values", "call") &&
         helpers.Member(next, "URLSearchParams Iterator", "next", "call");
}
}  // namespace ruyijs::web_platform