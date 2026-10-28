// Special HTMLDDA object; records, nodes, revisions and caches remain Collection-owned.
#include "document_model/state.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Symbol.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
CollectionRecord* Record(JSContext* cx, JS::HandleValue value) {
  JSObject* object = value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr;
  if (!object || JS::GetClass(object) != &kAllCollectionClass) {
    TypeError(cx, "HTMLAllCollection receiver is incompatible"); return nullptr;
  }
  auto* record = static_cast<CollectionRecord*>(JS::GetReservedSlot(object,0).toPrivate());
  if (!record || !record->dataOwner) { TypeError(cx,"HTMLAllCollection is detached"); return nullptr; }
  return record;
}
bool Lookup(JSContext* cx, const JS::CallArgs& args, CollectionRecord& record, bool named) {
  if (!args.length()) {
    if (named) return TypeError(cx,"namedItem requires a name");
    args.rval().setNull(); return true;
  }
  DomString name;
  if (!String(cx, args[0], &name)) return false;
  State& owner = *record.dataOwner;
  JS::RootedValue value(cx);
  {
    JSAutoRealm realm(cx, owner.page);
    if (!owner.SyncCollections() || !FreshCollection(owner, record)) return false;
    uint64_t index = 0;
    bool numeric = !name.empty() && (name.size() == 1 || name[0] != u'0') && name.size() <= 10;
    for (char16_t c : name) {
      if (c < u'0' || c > u'9') { numeric = false; break; }
      index = index * 10 + (c - u'0');
    }
    numeric = numeric && index < UINT32_MAX;
    if (!named && numeric) {
      if (index >= record.items.size()) value.setNull();
      else if (!CollectionIndex(owner,record,size_t(index),&value)) return false;
    } else if (!AllNameValue(owner,record,name,&value)) return false;
  }
  if (!JS_WrapValue(cx,&value)) return false;
  args.rval().set(value); return true;
}
bool LegacyCall(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc,vp);
  auto* record = Record(cx,args.calleev());
  return record && Lookup(cx,args,*record,false);
}
bool Member(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc,vp);
  const int operation = FunctionSlot(args);
  auto* record = Record(cx,args.thisv());
  if (!record) return false;
  if (operation == 2) {
    State& owner = *record->dataOwner;
    {
      JSAutoRealm realm(cx, owner.page);
      if (!owner.SyncCollections() || !FreshCollection(owner, *record)) return false;
    }
    args.rval().setNumber(double(record->items.size())); return true;
  }
  return Lookup(cx,args,*record,operation == 1);
}
bool All(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc,vp);
  Wrapper* node = NodeReceiver(cx,args.thisv());
  if (!node) return false;
  State& owner = *node->owner;
  JS::RootedValue value(cx);
  {
    JSAutoRealm realm(cx,owner.page);
    NodeSnapshot snapshot;
    if (!owner.Check(owner.model.Inspect(node->key,&snapshot))) return false;
    if (snapshot.kind != NodeKind::Document) return TypeError(cx,"Document.all receiver is incompatible");
    owner.Observe("Document","all",int(ApiAccess::Read));
    Collection* entry = owner.FindCollection(CollectionKind::HTMLCollection,node->key,CollectionFilter::All,u"");
    if (!entry) entry = owner.NewCollection(CollectionKind::HTMLCollection,node->key,CollectionFilter::All,u"",true);
    if (!entry || !owner.SyncCollections() || !FreshCollection(owner, *entry->record)) return false;
    JS::RootedObject source(cx,node->object);
    if (!JS_WrapObject(cx,&source)) return false;
    JS::SetReservedSlot(entry->object,2,JS::ObjectValue(*source));
    value.setObject(*entry->object);
  }
  if (!JS_WrapValue(cx,&value)) return false;
  args.rval().set(value); return true;
}
const JSClassOps ops = {nullptr,nullptr,CollectionEnumerate,nullptr,CollectionResolve,CollectionMayResolve,
                       FinalizeCollection,LegacyCall,nullptr,TraceCollection};
}
const JSClass kAllCollectionClass = {"HTMLAllCollection",
    JSCLASS_HAS_RESERVED_SLOTS(5) | JSCLASS_EMULATES_UNDEFINED | JSCLASS_FOREGROUND_FINALIZE, &ops};
bool InstallAllCollection(State& state) {
  JSContext* cx = state.cx;
  JS::RootedObject prototype(cx,state.Find("HTMLAllCollection")->prototype);
  JS::RootedValue constructor(cx);
  JS::RootedId tag(cx,JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx,JS::SymbolCode::toStringTag)));
  JS::RootedValue tagValue(cx);
  JS::ObjectOpResult removed;
  if (!JS_GetProperty(cx,prototype,"constructor",&constructor) ||
      !JS_GetPropertyById(cx,prototype,tag,&tagValue) ||
      !JS_DeleteProperty(cx,prototype,"constructor",removed) || !removed.ok() ||
      !JS_DeletePropertyById(cx,prototype,tag,removed) || !removed.ok()) return false;
  return state.Method("HTMLAllCollection","namedItem",Member,1,1) &&
      state.Method("HTMLAllCollection","item",Member,0,0) &&
      state.Accessor("HTMLAllCollection","length",Member,2) &&
      JS_DefineProperty(cx,prototype,"constructor",constructor,0) &&
      InstallCollectionIterator(state,"HTMLAllCollection",int32_t(CollectionIteration::Values)) &&
      JS_DefinePropertyById(cx,prototype,tag,tagValue,JSPROP_READONLY) &&
      state.Accessor("Document","all",All,0);
}
}
