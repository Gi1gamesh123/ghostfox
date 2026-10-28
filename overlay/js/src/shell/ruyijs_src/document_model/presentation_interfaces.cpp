// Document Model interfaces Firefox 158 exposes on Window whose instances come only
// from features this runtime does not run: CaretPosition (caretPositionFromPoint
// over a layout), PaintRequest/PaintRequestList (MozAfterPaint), ViewTransition
// and ViewTransitionTypeSet (startViewTransition rendering). The interface objects
// keep Firefox's shape; every member fails its brand check with Firefox's text and
// the prototype iterators are the list's own members (D149).
#include "document_model/state.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Symbol.h"
#include <iterator>
#include <string>

namespace ruyijs::document_model {
namespace {
struct Spec {
  const char* name;
  const char* parent;  // null: Object.prototype
  const char* methods[8];
  unsigned lengths[8];
  const char* getters[5];
  const char* iterator;  // the method @@iterator aliases, or null
};
const Spec kSpecs[] = {
    {"CaretPosition", nullptr, {"getClientRect"}, {0}, {"offsetNode", "offset"}, nullptr},
    {"PaintRequest", nullptr, {}, {}, {"clientRect", "reason"}, nullptr},
    {"PaintRequestList", nullptr, {"item"}, {1}, {"length"}, "@@array"},  // indexed: Array's iterator
    {"ViewTransition", nullptr, {"skipTransition"}, {0}, {"updateCallbackDone", "ready", "finished", "types"}, nullptr},
    {"ViewTransitionTypeSet", nullptr, {"add", "clear", "delete", "entries", "keys", "values", "forEach", "has"},
     {1, 0, 1, 0, 0, 0, 1, 1}, {"size"}, "values"},
};
// Slot: spec index * 32 + member (methods 0..7, getters 8..12).
bool Brand(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = FunctionSlot(args);
  const Spec& spec = kSpecs[slot / 32];
  const int member = slot % 32;
  const std::string label = member < 8 ? spec.methods[member] : std::string("get ") + spec.getters[member - 8];
  const bool ok = TypeError(cx, ("'" + label + "' called on an object that does not implement interface " +
                                 spec.name + ".").c_str());
  // ViewTransition's three Promise attributes report the failure as a rejection.
  if (std::string(spec.name) != "ViewTransition" || member < 8 || member > 10) return ok;
  JS::RootedValue error(cx);
  if (!JS_GetPendingException(cx, &error)) return false;
  JS_ClearPendingException(cx);
  JSObject* promise = JS::CallOriginalPromiseReject(cx, error);
  if (!promise) return false;
  args.rval().setObject(*promise);
  return true;
}
}  // namespace

bool InstallPresentationInterfaces(State& state) {
  JSContext* cx = state.cx;
  JS::RootedObject plain(cx, JS::GetRealmObjectPrototype(cx));
  for (int index = 0; index < int(std::size(kSpecs)); ++index) {
    const Spec& spec = kSpecs[index];
    if (!state.DefineInterface(spec.name, plain, Illegal, false)) return false;
    for (int m = 0; m < 8 && spec.methods[m]; ++m)
      if (!state.Method(spec.name, spec.methods[m], Brand, spec.lengths[m], index * 32 + m)) return false;
    for (int g = 0; g < 5 && spec.getters[g]; ++g)
      if (!state.Accessor(spec.name, spec.getters[g], Brand, index * 32 + 8 + g)) return false;
    if (spec.iterator) {
      JS::RootedObject prototype(cx, state.Find(spec.name)->prototype);
      JS::RootedValue method(cx);
      JS::RootedId id(cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::iterator)));
      JS::RootedObject source(cx, prototype);
      if (std::string(spec.iterator) == "@@array" && !JS_GetClassPrototype(cx, JSProto_Array, &source)) return false;
      if (!(source == prototype ? JS_GetProperty(cx, source, spec.iterator, &method)
                                : JS_GetPropertyById(cx, source, id, &method)) ||
          !JS_DefinePropertyById(cx, prototype, id, method, 0)) return false;
    }
  }
  return true;
}
}  // namespace ruyijs::document_model
