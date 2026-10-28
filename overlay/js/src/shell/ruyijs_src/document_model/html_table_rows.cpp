#include "document_model/html_table.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
bool CollectionGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  const int slot=FunctionSlot(args);
  auto* entry=TableReceiver(cx,args,TableBrand(slot/16));
  if (!entry) return false;
  auto& state=*entry->owner;
  auto filter=slot%16==1 ? CollectionFilter::TableBodies :
      slot/16==Row ? CollectionFilter::TableCells : CollectionFilter::TableRows;
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx,state.page);
    auto* list=state.FindCollection(CollectionKind::HTMLCollection,entry->key,filter,u"");
    if (!list) list=state.NewCollection(CollectionKind::HTMLCollection,entry->key,filter,u"",true);
    if (!list || !state.SyncCollections()) return false;
    // A live list keeps its scope reachable even when it contains no nodes.
    // The existing realm cache owns the list; this slot traces the original
    // wrapper and follows adoption/GC without another node identity.
    JS::SetReservedSlot(list->object,2,JS::ObjectValue(*entry->object));
    result.setObject(*list->object);
  }
  if (!JS_WrapValue(cx,&result)) return false;
  args.rval().set(result); return true;
}
bool IndexGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  const int kind=FunctionSlot(args);
  auto* entry=TableReceiver(cx,args,kind==2 ? Cell : Row);
  if (!entry) return false;
  int32_t index=-1;
  { JSAutoRealm realm(cx,entry->owner->page);
    if (!entry->owner->Check(entry->owner->model.TableIndex(entry->key,kind,&index))) return false; }
  args.rval().setInt32(index); return true;
}
bool Insert(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  auto brand=TableBrand(FunctionSlot(args));
  auto* entry=TableReceiver(cx,args,brand);
  if (!entry) return false;
  int32_t index=-1;
  if (!args.get(0).isUndefined() && !JS::ToInt32(cx,args[0],&index)) return false;
  auto& state=*entry->owner;
  JS::RootedValue result(cx),body(cx);
  {
    JSAutoRealm realm(cx,state.page);
    TableInsertion plan; NodeKey made,bodyKey;
    if (!state.Check(state.model.TableInsertPlan(entry->key,index,&plan))) return false;
    if (plan.body && !TableCreate(state,entry->key,u"tbody",&body,&bodyKey)) return false;
    if (!TableCreate(state,entry->key,brand==Row?u"td":u"tr",&result,&made)) return false;
    if (plan.body) {
      if (!TableMove(state,entry->key,bodyKey,{})) return false;
      plan.parent=bodyKey;
    }
    if (!TableMove(state,plan.parent,made,plan.before)) return false;
  }
  if (!JS_WrapValue(cx,&result)) return false;
  args.rval().set(result); return true;
}
bool Delete(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  auto* entry=TableReceiver(cx,args,TableBrand(FunctionSlot(args)));
  if (!entry) return false;
  if (!argc) return TypeError(cx,"Table deletion requires an index");
  int32_t index;
  if (!JS::ToInt32(cx,args[0],&index)) return false;
  auto& state=*entry->owner;
  {
    JSAutoRealm realm(cx,state.page);
    NodeKey target; NodeSnapshot node;
    if (!state.Check(state.model.TableDeleteTarget(entry->key,index,&target))) return false;
    if (target.node && (!state.Check(state.model.Inspect(target,&node)) ||
        !state.Check(state.model.Remove(node.parent,target)))) return false;
  }
  args.rval().setUndefined(); return true;
}
}
bool InstallTableRows(State& state) {
  for (auto brand:{Table,Section,Row}) {
    const char* iface=TableInterface(brand);
    if (!state.Accessor(iface,brand==Row?"cells":"rows",CollectionGet,int(brand)*16) ||
        !state.Method(iface,brand==Row?"insertCell":"insertRow",Insert,0,brand) ||
        !state.Method(iface,brand==Row?"deleteCell":"deleteRow",Delete,1,brand)) return false;
  }
  return state.Accessor(TableInterface(Table),"tBodies",CollectionGet,1) &&
      state.Accessor(TableInterface(Row),"rowIndex",IndexGet,0) &&
      state.Accessor(TableInterface(Row),"sectionRowIndex",IndexGet,1) &&
      state.Accessor(TableInterface(Cell),"cellIndex",IndexGet,2);
}
}
