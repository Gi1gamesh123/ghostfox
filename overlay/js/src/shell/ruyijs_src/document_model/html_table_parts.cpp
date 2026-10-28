#include "document_model/html_table.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
const char16_t* const kTags[]={u"caption",u"thead",u"tfoot",u"tbody"};
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  int slot=FunctionSlot(args);
  auto* entry=TableReceiver(cx,args,Table);
  if (!entry) return false;
  auto& state=*entry->owner;
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx,state.page);
    NodeKey part;
    if (!state.Check(state.model.TablePart(entry->key,kTags[slot],&part,nullptr)) ||
        !state.Wrap(part,&result)) return false;
  }
  if (!JS_WrapValue(cx,&result)) return false;
  args.rval().set(result); return true;
}
bool Create(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  int slot=FunctionSlot(args);
  auto* entry=TableReceiver(cx,args,Table);
  if (!entry) return false;
  auto& state=*entry->owner;
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx,state.page);
    NodeKey part,before;
    if (!state.Check(state.model.TablePart(entry->key,kTags[slot],&part,&before))) return false;
    if (part.node && slot!=3) {
      if (!state.Wrap(part,&result)) return false;
    } else if (!TableCreate(state,entry->key,kTags[slot],&result,&part) ||
               !TableMove(state,entry->key,part,before)) return false;
  }
  if (!JS_WrapValue(cx,&result)) return false;
  args.rval().set(result); return true;
}
bool Delete(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  int slot=FunctionSlot(args);
  auto* entry=TableReceiver(cx,args,Table);
  if (!entry) return false;
  auto& state=*entry->owner;
  {
    JSAutoRealm realm(cx,state.page);
    NodeKey part;
    if (!state.Check(state.model.TablePart(entry->key,kTags[slot],&part,nullptr)) ||
        (part.node && !state.Check(state.model.Remove(entry->key,part)))) return false;
  }
  args.rval().setUndefined(); return true;
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  int slot=FunctionSlot(args);
  auto* entry=TableReceiver(cx,args,Table);
  if (!entry) return false;
  auto& state=*entry->owner;
  JS::RootedValue value(cx,args.get(0));
  {
    JSAutoRealm realm(cx,state.page);
    if (!JS_WrapValue(cx,&value)) return false;
    NodeKey replacement,part,before;
    if (!NodeArgument(state,value,&replacement,true)) return false;
    if (replacement.node) {
      NodeSnapshot node;
      if (!state.Check(state.model.Inspect(replacement,&node))) return false;
      const auto* expected=TableInterface(slot==0?Caption:Section);
      if (std::string_view(node.interface_name)!=expected)
        return TypeError(cx,"Table section setter received an incompatible node");
      if (node.local_name!=kTags[slot])
        return ThrowDOM(state,"HierarchyRequestError","Table section has the wrong tag");
      bool ancestor=false;
      if (!state.Check(state.model.Contains(replacement,entry->key,&ancestor))) return false;
      if (ancestor) return ThrowDOM(state,"HierarchyRequestError","Table section would create a cycle");
    }
    if (!state.Check(state.model.TablePart(entry->key,kTags[slot],&part,nullptr)) ||
        (part.node && !state.Check(state.model.Remove(entry->key,part)))) return false;
    if (replacement.node &&
        (!state.Check(state.model.TablePart(entry->key,kTags[slot],nullptr,&before)) ||
         !TableMove(state,entry->key,replacement,before))) return false;
  }
  args.rval().setUndefined(); return true;
}
}
bool InstallTableParts(State& state) {
  const char* const attrs[]={"caption","tHead","tFoot"};
  const char* const create[]={"createCaption","createTHead","createTFoot","createTBody"};
  const char* const remove[]={"deleteCaption","deleteTHead","deleteTFoot"};
  for (int i=0;i<4;++i) {
    if (!state.Method(TableInterface(Table),create[i],Create,0,i)) return false;
    if (i<3 && (!state.Method(TableInterface(Table),remove[i],Delete,0,i) ||
        !state.Accessor(TableInterface(Table),attrs[i],Get,i,Set))) return false;
  }
  return true;
}
}
