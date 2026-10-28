#include "document_model/state.h"
#include "document_model/foreign_adoption.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Symbol.h"

namespace ruyijs::document_model {
namespace {
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  NodeKind kind = NodeKind(FunctionSlot(args));
  auto* entry = Receiver(state, args, &kind);
  if (!entry) return false;
  if (kind == NodeKind::DocumentFragment && !state.Check(state.model.ClearFragment(entry->key))) return false;
  args.rval().setUndefined(); return true;
}
bool ReplaceChildren(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const NodeKind kind = NodeKind(FunctionSlot(args));
  auto* entry = NodeReceiver(cx, args.thisv(), kind);
  if (!entry) return false;
  entry->owner->Observe(kind == NodeKind::Element ? "Element" : kind == NodeKind::Document
      ? "Document" : "DocumentFragment", "replaceChildren", int(ApiAccess::Call));
  if (!ReplaceNodeChildren(*entry, args)) return false;
  args.rval().setUndefined(); return true;
}
bool Unscopables(State& state, const char* iface, const char* const* keys, size_t length) {
  JS::RootedObject target(state.cx, state.Find(iface)->prototype);
  JS::RootedObject object(state.cx, JS_NewObjectWithGivenProto(state.cx, nullptr, nullptr));
  JS::RootedValue value(state.cx, JS::BooleanValue(true));
  JS::RootedId id(state.cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(state.cx, JS::SymbolCode::unscopables)));
  if (!object) return false;
  for (size_t i = 0; i < length; ++i)
    if (!JS_DefineProperty(state.cx, object, keys[i], value, JSPROP_ENUMERATE)) return false;
  return JS_DefinePropertyById(state.cx, target, id, object, JSPROP_READONLY);
}
}
bool InstallParentMembers(State& state) {
  static const char* character[] = {"after", "before", "remove", "replaceWith"};
  static const char* element[] = {"after", "append", "before", "prepend", "remove", "replaceChildren", "replaceWith", "slot"};
  static const char* fragment[] = {"append", "prepend", "replaceChildren"};
  static const char* document[] = {"append", "fullscreen", "prepend", "replaceChildren"};
  return state.Method("Element", "replaceChildren", ReplaceChildren, 0, int32_t(NodeKind::Element)) &&
      state.Method("Element", "moveBefore", Call, 2, int32_t(NodeKind::Element)) &&
      state.Method("Document", "replaceChildren", ReplaceChildren, 0, int32_t(NodeKind::Document)) &&
      state.Method("Document", "moveBefore", Call, 2, int32_t(NodeKind::Document)) &&
      state.Method("DocumentFragment", "replaceChildren", ReplaceChildren, 0, int32_t(NodeKind::DocumentFragment)) &&
      Unscopables(state, "CharacterData", character, std::size(character)) &&
      Unscopables(state, "Element", element, std::size(element)) &&
      Unscopables(state, "DocumentFragment", fragment, std::size(fragment)) &&
      Unscopables(state, "Document", document, std::size(document));
}
}
