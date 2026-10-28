#include "document_model/state.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Symbol.h"

namespace ruyijs::document_model {
namespace {
Wrapper* Receiver(JSContext* cx, JS::HandleValue value, NodeSnapshot* snapshot) {
  auto* entry = NodeReceiver(cx, value);
  if (!entry || !entry->owner->Check(entry->owner->model.Inspect(entry->key, snapshot))) return nullptr;
  if (snapshot->kind == NodeKind::DocumentType) return entry;
  TypeError(cx, "DocumentType receiver is incompatible"); return nullptr;
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int field = FunctionSlot(args);
  NodeSnapshot node;
  if (!Receiver(cx, args.thisv(), &node)) return false;
  constexpr const char* fields[] = {"name", "publicId", "systemId"};
  FunctionState(args)->Observe("DocumentType", fields[field], int(ApiAccess::Read));
  return Value(cx, DomStringView(field == 0 ? node.node_name : field == 1 ? node.public_id : node.system_id), args.rval());
}
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int method = FunctionSlot(args);
  NodeSnapshot node;
  auto* entry = Receiver(cx, args.thisv(), &node);
  if (!entry) return false;
  constexpr const char* names[] = {"after", "before", "remove", "replaceWith"};
  FunctionState(args)->Observe("DocumentType", names[method], int(ApiAccess::Call));
  // The public ChildNode methods skip all conversion when detached.
  if (Same(node.parent, {})) { args.rval().setUndefined(); return true; }
  State& owner = *entry->owner;
  std::vector<Item> items;
  if (method != 2) {
    items.reserve(argc);
    for (unsigned i = 0; i < argc; ++i) {
      Item item;
      if (args[i].isString()) {
        if (!String(cx, args[i], &item.text)) return false;
      } else if (!NodeArgument(owner, args[i], &item.node)) return false;
      items.push_back(std::move(item));
    }
  }
  static constexpr InsertMode modes[] = {InsertMode::After, InsertMode::Before, InsertMode::Detach, InsertMode::ReplaceWith};
  std::vector<std::pair<NodeKey, NodeKey>> rekey;
  const Status status = owner.model.InsertItems(entry->key, modes[method], items, &rekey);
  owner.Rekey(rekey);
  if (!owner.Check(status)) return false;
  args.rval().setUndefined(); return true;
}
}
bool InstallDocumentType(State& state) {
  constexpr const char* fields[] = {"name", "publicId", "systemId"};
  constexpr const char* methods[] = {"after", "before", "remove", "replaceWith"};
  for (int i = 0; i < 3; ++i)
    if (!state.Accessor("DocumentType", fields[i], Get, i)) return false;
  JS::RootedObject prototype(state.cx, state.Find("DocumentType")->prototype);
  JS::RootedObject unscopables(state.cx, JS_NewObjectWithGivenProto(state.cx, nullptr, nullptr));
  if (!unscopables) return false;
  JS::RootedValue value(state.cx, JS::BooleanValue(true));
  for (int i = 0; i < 4; ++i)
    if (!state.Method("DocumentType", methods[i], Call, 0, i) ||
        !JS_DefineProperty(state.cx, unscopables, methods[i], value, JSPROP_ENUMERATE)) return false;
  JS::RootedId id(state.cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(state.cx, JS::SymbolCode::unscopables)));
  return JS_DefinePropertyById(state.cx, prototype, id, unscopables, JSPROP_READONLY) &&
      state.Method("DOMImplementation", "createDocumentType", CreateDocumentType, 3, 0);
}
}
