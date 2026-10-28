// Original document reference and wrapper cache; first lookup uses the method realm.
#include "document_model/state.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
namespace ruyijs::document_model {
namespace {
bool Cache(State& state, JS::HandleObject document, JS::HandleObject type) {
  JS::RootedValue value(state.cx, JS::ObjectValue(*type));
  JSAutoRealm realm(state.cx, document);
  if (!JS_WrapValue(state.cx, &value)) return false;
  JS::SetReservedSlot(document, 7, value); return true;
}
}
bool ParsedDocumentTypeGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& factory = *FunctionState(args);
  JS::RootedObject source(cx, args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  auto* entry = DocumentReceiver(cx, source, &args);
  if (!entry) return false;
  factory.Observe("Document", "doctype", int(ApiAccess::Read));
  NodeKey type;
  State& owner = *entry->owner;
  return owner.Check(owner.model.ParsedDocumentType(entry->key, &type)) && owner.Wrap(type, args.rval(), nullptr, &factory);
}
bool CacheParsedDocumentType(State& state, NodeKey key, NodeKind kind, JS::HandleObject object) {
  if (kind == NodeKind::Document) {
    NodeKey type;
    if (!state.Check(state.model.ParsedDocumentType(key, &type))) return false;
    if (auto* entry = state.Find(type)) {
      JS::RootedObject value(state.cx, entry->object);
      return Cache(state, object, value);
    }
  } else if (kind == NodeKind::DocumentType) {
    std::vector<NodeKey> owners;
    if (!state.Check(state.model.ParsedDocumentTypeOwners(key, &owners))) return false;
    for (NodeKey document : owners) if (auto* entry = state.Find(document)) {
      JS::RootedObject source(state.cx, entry->object);
      if (!Cache(state, source, object)) return false;
    }
  }
  return true;
}
}
