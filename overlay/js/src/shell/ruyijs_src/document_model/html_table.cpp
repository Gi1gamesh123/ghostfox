#include "document_model/html_table.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include <cstring>

namespace ruyijs::document_model {
const char* TableInterface(TableBrand kind) {
  static const char* const names[]={"HTMLTableElement","HTMLTableSectionElement",
      "HTMLTableRowElement","HTMLTableCellElement","HTMLTableColElement","HTMLTableCaptionElement"};
  return names[kind];
}
Wrapper* TableReceiver(JSContext* cx, const JS::CallArgs& args, TableBrand kind) {
  JS::RootedObject object(cx,args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  auto* entry=object && JS::GetClass(object)==&kNodeClass
      ? static_cast<Wrapper*>(JS::GetReservedSlot(object,0).toPrivate()) : nullptr;
  if (!entry || entry->dead || !entry->owner) {
    TypeError(cx,"Table member called on incompatible receiver"); return nullptr;
  }
  NodeSnapshot node;
  auto& state=*entry->owner;
  JSAutoRealm realm(cx,state.page);
  if (!state.Check(state.model.Inspect(entry->key,&node))) return nullptr;
  if (std::strcmp(node.interface_name,TableInterface(kind))) {
    TypeError(cx,"Table member called on incompatible receiver"); return nullptr;
  }
  return entry;
}
bool TableCreate(State& state, NodeKey scope, DomStringView tag,
                 JS::MutableHandleValue value, NodeKey* key) {
  NodeSnapshot node; Draft draft;
  if (!state.Check(state.model.Inspect(scope,&node)) ||
      !state.Check(state.model.Create(node.owner_document,{NodeKind::Element,tag,{}},&draft)) ||
      !state.Commit(draft,value)) return false;
  *key=draft.root;
  return true;
}
bool TableMove(State& state, NodeKey parent, NodeKey child, NodeKey before) {
  std::vector<std::pair<NodeKey,NodeKey>> rekey;
  Status result=state.model.Insert(parent,child,before,&rekey);
  state.Rekey(rekey);
  return state.Check(result);
}
bool InstallTableElements(State& state) {
  return InstallTableRows(state) && InstallTableParts(state) && InstallTableAttributes(state);
}
}
