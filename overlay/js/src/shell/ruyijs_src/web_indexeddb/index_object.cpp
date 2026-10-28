// IDBIndex (docs/owners/INDEXED_DB.md, 2026-10-21): attributes read the one
// Catalog's index definition; queries are request jobs over the index rows;
// name setters rename in the upgrade only. Also IDBObjectStore.name's setter.
#include "web_indexeddb/keys.h"
#include "js/MapAndSet.h"
#include "js/Object.h"
#include "js/Wrapper.h"

namespace ruyijs::web_indexeddb {
namespace {
enum Field { FName, FObjectStore, FKeyPath, FMultiEntry, FUnique };
State* Self(JSContext* cx, const JS::CallArgs& args, JS::MutableHandleObject out) {
  JS::HandleValue value = args.thisv();
  out.set(value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  if (!out || JS::GetClass(out) != &kIndexClass) { WebKit::FailBrand(cx, args, "IDBIndex"); return nullptr; }
  return static_cast<State*>(JS::GetReservedSlot(out, IndexOwner).toPrivate());
}
// The index definition, its store's connection key and name, and the store's transaction.
bool Lookup(State& s, JS::HandleObject index, IndexInfo* info, Key* database, std::u16string* storeName,
            JS::MutableHandleObject tx) {
  JSContext* cx = s.cx;
  JS::RootedObject store(cx, &JS::GetReservedSlot(index, IndexStore).toObject());
  JS::RootedValue name(cx, JS::GetReservedSlot(index, IndexName)), storeValue(cx, JS::GetReservedSlot(store, StoreName));
  std::u16string wanted;
  std::vector<IndexInfo> indexes;
  if (!Text(cx, name, &wanted) || !Text(cx, storeValue, storeName)) return false;
  if (!StoreConnection(s, store, database, tx) || !s.store->model.Indexes(*database, *storeName, &indexes))
    return Throw(s, "InvalidStateError");
  for (const auto& entry : indexes) if (entry.name == wanted) { *info = entry; return true; }
  return Throw(s, "InvalidStateError");  // deleted index
}
bool Upgrading(JS::HandleObject store) {
  return JS::GetReservedSlot(store, StoreTransaction).isNull() &&
         JS::GetReservedSlot(&JS::GetReservedSlot(store, StoreDatabase).toObject(), 8).toBoolean();
}
}  // namespace

bool IndexGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  State* s = Self(cx, args, &self);
  if (!s) return false;
  const int field = WebKit::Slot(args);
  if (field == FName || field == FObjectStore) {
    args.rval().set(JS::GetReservedSlot(self, field == FName ? IndexName : IndexStore));
    return JS_WrapValue(cx, args.rval());
  }
  IndexInfo info;
  Key database;
  std::u16string storeName;
  JS::RootedObject tx(cx);
  if (!Lookup(*s, self, &info, &database, &storeName, &tx)) return false;
  if (field == FMultiEntry || field == FUnique) { args.rval().setBoolean(field == FUnique ? info.unique : info.multiEntry); return true; }
  if (JS::GetReservedSlot(self, IndexKeyPath).isUndefined()) {  // one keyPath object per index
    JS::RootedValue path(cx);
    if (!KeyPathValue(*s, info, &path)) return false;
    JS::SetReservedSlot(self, IndexKeyPath, path);
  }
  args.rval().set(JS::GetReservedSlot(self, IndexKeyPath));
  return JS_WrapValue(cx, args.rval());
}
bool IndexSetName(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  State* s = Self(cx, args, &self);
  if (!s) return false;
  std::u16string to;
  IndexInfo info;
  Key database;
  std::u16string storeName;
  JS::RootedObject tx(cx), store(cx, &JS::GetReservedSlot(self, IndexStore).toObject());
  if (!Text(cx, args.get(0), &to) || !Lookup(*s, self, &info, &database, &storeName, &tx)) return false;
  if (!Upgrading(store)) return Throw(*s, "InvalidStateError");
  args.rval().setUndefined();
  if (to == info.name) return true;
  if (!s->store->model.RenameIndex(database, storeName, info.name, to)) return Throw(*s, "ConstraintError");
  JS::RootedObject cache(cx, &JS::GetReservedSlot(store, StoreIndexes).toObject());
  JS::RootedValue from(cx, JS::GetReservedSlot(self, IndexName)), next(cx), object(cx, JS::ObjectValue(*self));
  bool removed = false;
  if (!String(cx, to, &next) || !JS::MapDelete(cx, cache, from, &removed) || !JS::MapSet(cx, cache, next, object)) return false;
  JS::SetReservedSlot(self, IndexName, next);
  return true;
}
bool StoreSetName(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx, args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!self || JS::GetClass(self) != &kStoreClass) return WebKit::FailBrand(cx, args, "IDBObjectStore");
  State& s = *static_cast<State*>(JS::GetReservedSlot(self, StoreOwner).toPrivate());
  std::u16string to, from;
  Key database;
  JS::RootedObject tx(cx);
  JS::RootedValue current(cx, JS::GetReservedSlot(self, StoreName));
  if (!Text(cx, args.get(0), &to) || !Text(cx, current, &from)) return false;
  if (!Upgrading(self) || !StoreConnection(s, self, &database, &tx)) return Throw(s, "InvalidStateError");
  args.rval().setUndefined();
  if (to == from) return true;
  if (!s.store->model.RenameStore(database, from, to)) return Throw(s, "ConstraintError");
  JS::RootedValue next(cx);
  if (!String(cx, to, &next)) return false;
  JS::SetReservedSlot(self, StoreName, next);
  return true;
}
// get/getKey/getAll/getAllKeys/count/getAllRecords and openCursor/openKeyCursor on an index.
bool IndexMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  State* s = Self(cx, args, &self);
  if (!s) return false;
  const int op = WebKit::Slot(args);
  IndexInfo info;
  Key database;
  std::u16string storeName;
  JS::RootedObject tx(cx);
  if (op >= 26) {  // openCursor (26) / openKeyCursor (27)
    if (!Lookup(*s, self, &info, &database, &storeName, &tx)) return false;
    if (!tx) return Throw(*s, "TransactionInactiveError");
    return OpenCursor(*s, self, tx, &info.name, args.get(0), args.get(1), op == 27, args.rval());
  }
  Query query;
  JS::RootedValue queryInput(cx);
  if (!PrepareQuery(cx, args, op, &query, &queryInput) || !Lookup(*s, self, &info, &database, &storeName, &tx)) return false;
  if (!tx || JS::GetReservedSlot(tx, TxActive).toInt32()) return Throw(*s, "TransactionInactiveError");
  if (!ReadQuery(*s, queryInput, op, &query)) return false;
  if (op == Get || op == GetKey) query.count = 1;
  {
    JSAutoRealm realm(cx, self);
    JS::RootedObject request(cx, NewRequest(*s, self, tx));
    if (!request) return false;
    auto job = std::make_unique<Job>(s, cx, request, Key{}, 0, false);
    job->kind = op;
    job->query = std::move(query);
    job->hasIndex = true;
    job->index = info.name;
    if (!EnqueueJob(*s, std::move(job), tx)) return false;
    args.rval().setObject(*request);
  }
  return JS_WrapValue(cx, args.rval());
}
}  // namespace ruyijs::web_indexeddb
