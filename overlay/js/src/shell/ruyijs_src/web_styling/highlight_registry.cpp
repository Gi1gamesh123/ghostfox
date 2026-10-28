// HighlightRegistry: maplike<DOMString, Highlight> plus highlightsFromPoint, behind
// CSS.highlights (the realm's one instance, a getter on the CSS namespace object).
// Slot 0 is the backing engine Map; the members share Highlight's collection body.
#include "web_styling/highlight.h"
#include "jsfriendapi.h"
#include "js/MapAndSet.h"
#include "runtime/page_helpers.h"
#include "js/Wrapper.h"
#include "web_styling/internal.h"
#include <iterator>

namespace ruyijs::web_styling {
namespace {
const JSClass kRegistryClass = {"HighlightRegistry", JSCLASS_HAS_RESERVED_SLOTS(1)};
const char* const kNames[] = {"set", "clear", "delete", "highlightsFromPoint", "entries", "keys", "values",
                              "forEach", "has", "get"};
const unsigned kLengths[] = {2, 0, 1, 2, 0, 0, 0, 1, 1, 1};
bool Member(JSContext* cx, unsigned argc, JS::Value* vp) {
  const int op = WebKit::Slot(JS::CallArgsFromVp(argc, vp));
  return CollectionMember(cx, argc, vp, &kRegistryClass, "HighlightRegistry",
                          op < int(std::size(kNames)) ? kNames[op] : "size");
}
bool Illegal(JSContext* cx, unsigned, JS::Value*) { return WebKit::Fail(cx, "Illegal constructor."); }
// CSS.highlights: [SameObject], the registry in the getter's reserved slot.
bool Highlights(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  args.rval().set(js::GetFunctionNativeReserved(&args.callee(), 1));
  return true;
}
}  // namespace

bool InstallHighlightRegistry(State& s, JS::HandleObject css) {
  WebKit& kit = s.kit;
  JSContext* cx = s.cx;
  JS::RootedObject objectProto(cx, JS::GetRealmObjectPrototype(cx));
  if (!kit.Define("HighlightRegistry", &kRegistryClass, objectProto, Illegal, &s.highlightRegistry)) return false;
  for (int i = 0; i < int(std::size(kNames)); ++i)
    if (!kit.Method(s.highlightRegistry, "HighlightRegistry", kNames[i], Member, kLengths[i], i)) return false;
  if (!kit.Accessor(s.highlightRegistry, "HighlightRegistry", "size", Member, nullptr, int(std::size(kNames))))
    return false;
  JS::RootedObject proto(cx, s.highlightRegistry.prototype);
  JS::RootedValue entries(cx);
  JS::RootedId iterator(cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::iterator)));
  if (!JS_GetProperty(cx, proto, "entries", &entries) || !JS_DefinePropertyById(cx, proto, iterator, entries, 0))
    return false;
  JS::RootedObject map(cx, JS::NewMapObject(cx));
  JS::RootedObject registry(cx, map ? kit.New(&kRegistryClass, proto) : nullptr);
  JS::RootedObject getter(cx, registry ? kit.Function(Highlights, "get highlights", 0, 0) : nullptr);
  if (!getter) return false;
  JS::SetReservedSlot(registry, 0, JS::ObjectValue(*map));
  js::SetFunctionNativeReserved(getter, 1, JS::ObjectValue(*registry));
  JS::RootedObject setter(cx);
  return JS_DefineProperty(cx, css, "highlights", getter, setter, JSPROP_ENUMERATE) &&
         kit.helpers->Accessor("CSS", css, "highlights");
}
}  // namespace ruyijs::web_styling
