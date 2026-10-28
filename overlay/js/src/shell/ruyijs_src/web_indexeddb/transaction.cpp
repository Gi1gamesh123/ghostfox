#include "web_indexeddb/state.h"
#include <algorithm>
#include "js/Array.h"
#include "js/MapAndSet.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
namespace ruyijs::web_indexeddb {
namespace {
bool Init(State& s,JS::HandleObject tx,JS::HandleObject db,uint32_t id,const char16_t* modeText,
          JS::HandleValue store) {
  JS::RootedValue mode(s.cx);
  JS::RootedObject stores(s.cx,JS::NewMapObject(s.cx));
  if(!stores || !s.events.Attach(tx) || !String(s.cx,modeText,&mode))return false;
  JS::SetReservedSlot(tx,TxOwner,JS::PrivateValue(&s));
  JS::SetReservedSlot(tx,TxId,JS::Int32Value(id));
  JS::SetReservedSlot(tx,TxDatabase,JS::ObjectValue(*db));
  JS::SetReservedSlot(tx,TxMode,mode);JS::SetReservedSlot(tx,TxStore,store);
  JS::SetReservedSlot(tx,TxActive,JS::Int32Value(0));JS::SetReservedSlot(tx,TxError,JS::NullValue());
  JS::SetReservedSlot(tx,TxStores,JS::ObjectValue(*stores));
  JS::SetReservedSlot(tx,TxIds,JS::NullValue());
  for(int i=0;i<3;++i)JS::SetReservedSlot(tx,TxHandler+i,JS::NullValue());
  return true;
}
// The scope names of a data transaction (TxStore) and the catalog id of each.
bool Scope(JSContext* cx,JS::HandleObject tx,std::vector<std::u16string>* names,std::vector<uint32_t>* ids) {
  JS::RootedValue list(cx,JS::GetReservedSlot(tx,TxStore)),idList(cx,JS::GetReservedSlot(tx,TxIds));
  if(!list.isObject() || !idList.isObject())return true;
  JS::RootedObject array(cx,&list.toObject()),idArray(cx,&idList.toObject());
  uint32_t length=0;if(!JS::GetArrayLength(cx,array,&length))return false;
  JS::RootedValue value(cx);
  for(uint32_t i=0;i<length;++i) {
    std::u16string name;
    if(!JS_GetElement(cx,array,i,&value) || !Text(cx,value,&name))return false;
    if(names)names->push_back(std::move(name));
    if(!JS_GetElement(cx,idArray,i,&value))return false;
    if(ids)ids->push_back(uint32_t(value.toInt32()));
  }
  return true;
}
}
bool EndTransaction(State& s,JS::HandleObject tx,bool commit) {
  std::vector<uint32_t> ids;
  if(!Scope(s.cx,tx,nullptr,&ids))return false;
  return ids.empty() || s.store->model.EndAll(ids,commit);
}
JSObject* NewTransaction(State& s,JS::HandleObject db,const std::vector<std::u16string>& names,bool write) {
  if(s.store->jobs.size()>=1024){Throw(s,"QuotaExceededError");return nullptr;}
  Connection* connection=FindConnection(s,db);if(!connection)return nullptr;
  std::vector<uint32_t> ids;
  for(const auto& name:names) {
    uint32_t id=s.store->model.Begin(connection->key,name,write);
    if(!id){if(!ids.empty())s.store->model.EndAll(ids,false);Throw(s,"QuotaExceededError");return nullptr;}
    ids.push_back(id);
  }
  JS::RootedObject tx(s.cx,s.kit.New(&kTransactionClass,s.transactionFace.prototype));
  JS::RootedObject scope(s.cx,JS::NewArrayObject(s.cx,0)),idArray(s.cx,JS::NewArrayObject(s.cx,0));
  JS::RootedValue value(s.cx);
  auto fail=[&]{s.store->model.EndAll(ids,false);return nullptr;};
  if(!tx || !scope || !idArray)return fail();
  for(uint32_t i=0;i<names.size();++i)
    if(!String(s.cx,names[i],&value) || !JS_SetElement(s.cx,scope,i,value) ||
       !JS_SetElement(s.cx,idArray,i,int32_t(ids[i])))return fail();
  value.setObject(*scope);
  if(!Init(s,tx,db,ids.front(),write?u"readwrite":u"readonly",value))return fail();
  JS::SetReservedSlot(tx,TxIds,JS::ObjectValue(*idArray));
  auto job=std::make_unique<Job>(&s,s.cx,tx,connection->key,0,false);job->kind=3;
  s.store->jobs.push_back(std::move(job));
  if(!Schedule(*s.store))return nullptr;return tx;
}
JSObject* NewVersionTransaction(State& s,JS::HandleObject db) {
  JS::RootedObject tx(s.cx,s.kit.New(&kTransactionClass,s.transactionFace.prototype));
  JS::RootedValue none(s.cx,JS::NullValue());
  if(!tx || !Init(s,tx,db,0,u"versionchange",none))return nullptr;
  return tx;
}
// One IDBObjectStore per store name and transaction (Firefox returns the same
// object). Upgrade stores keep a null StoreTransaction, the upgrade marker.
JSObject* TransactionStore(State& s,JS::HandleObject tx,const std::u16string& name,const std::u16string& path,
                           JS::HandleObject db,JS::HandleObject created) {
  JS::RootedObject cache(s.cx,&JS::GetReservedSlot(tx,TxStores).toObject());
  JS::RootedValue key(s.cx),found(s.cx);
  if(!String(s.cx,name,&key) || !JS::MapGet(s.cx,cache,key,&found))return nullptr;
  if(found.isObject() && !created)return &found.toObject();
  const bool version=!JS::GetReservedSlot(tx,TxId).toInt32();
  JS::RootedObject owner(s.cx,version?nullptr:tx.get());
  JS::RootedObject store(s.cx,created?created.get():NewObjectStore(s,owner,name,path,db));
  if(!store)return nullptr;
  if(!version && !created) {
    std::vector<std::u16string> names;std::vector<uint32_t> ids;
    if(!Scope(s.cx,tx,&names,&ids))return nullptr;
    for(size_t i=0;i<names.size();++i)
      if(names[i]==name) JS::SetReservedSlot(store,StoreTxId,JS::Int32Value(int32_t(ids[i])));
  }
  JS::RootedValue value(s.cx,JS::ObjectValue(*store));
  if(!JS::MapSet(s.cx,cache,key,value))return nullptr;
  return store;
}
bool TransactionGet(JSContext* cx,unsigned n,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(n,vp);JS::RootedObject self(cx);
  State* s=Receiver(cx,args,&kTransactionClass,&self);if(!s)return false;
  const int slot=WebKit::Slot(args);  // read before rval(), which shares the callee slot
  if(slot==TxDurability) {
    JSString* text=JS_NewStringCopyZ(cx,"default");if(!text)return false;
    args.rval().setString(text);return true;
  }
  if(slot!=TxStores) {args.rval().set(JS::GetReservedSlot(self,slot));return true;}
  // objectStoreNames: the scope, or every store of the database in the upgrade.
  std::vector<std::u16string> names;
  JS::RootedValue scope(cx,JS::GetReservedSlot(self,TxStore));
  if(scope.isObject()) {
    if(!Scope(cx,self,&names,nullptr))return false;
  } else {
    JS::RootedObject db(cx,&JS::GetReservedSlot(self,TxDatabase).toObject());
    Connection* c=FindConnection(*s,db);
    if(c && !s->store->model.Stores(c->key,&names))return Throw(*s,"InvalidStateError");
  }
  JS::RootedObject array(cx,JS::NewArrayObject(cx,0));JS::RootedValue value(cx);
  if(!array)return false;
  for(uint32_t i=0;i<names.size();++i)
    if(!String(cx,names[i],&value) || !JS_SetElement(cx,array,i,value))return false;
  JS::RootedObject list(cx,s->environment.StringList(array));if(!list)return false;
  args.rval().setObject(*list);return true;
}
bool TransactionMethod(JSContext* cx,unsigned n,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(n,vp);JS::RootedObject self(cx);
  State* s=Receiver(cx,args,&kTransactionClass,&self);if(!s)return false;
  if(JS::GetReservedSlot(self,TxActive).toInt32())return Throw(*s,"InvalidStateError");
  // The upgrade's transaction: abort is reported when the upgrade event returns
  // (complete.cpp); objectStore names any store of the database.
  const bool version=!JS::GetReservedSlot(self,TxId).toInt32();
  const int op=WebKit::Slot(args);
  if(op==1) {
    if(!version)EndTransaction(*s,self,false);
    JS::SetReservedSlot(self,TxActive,JS::Int32Value(2));args.rval().setUndefined();return true;
  }
  if(op==2) {
    // commit(): no new requests; queued ones finish, then the transaction commits.
    JS::SetReservedSlot(self,TxActive,JS::Int32Value(3));args.rval().setUndefined();return true;
  }
  if(!n)return WebKit::Fail(cx,"IndexedDB objectStore requires a name");
  std::u16string name,path;
  std::vector<std::u16string> scope;
  if(!Text(cx,args[0],&name) || (!version && !Scope(cx,self,&scope,nullptr)))return false;
  if(!version && std::find(scope.begin(),scope.end(),name)==scope.end())return Throw(*s,"NotFoundError");
  JS::RootedObject db(cx,&JS::GetReservedSlot(self,TxDatabase).toObject()),none(cx);
  Connection* c=FindConnection(*s,db);
  if(!c || !s->store->model.Path(c->key,name,&path))return Throw(*s,"NotFoundError");
  JS::RootedObject result(cx,TransactionStore(*s,self,name,path,db,none));if(!result)return false;
  args.rval().setObject(*result);return true;
}
}
