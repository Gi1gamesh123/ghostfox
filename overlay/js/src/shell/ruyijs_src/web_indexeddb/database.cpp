#include <algorithm>
#include "web_indexeddb/state.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
namespace ruyijs::web_indexeddb {
Connection* FindConnection(State& s,JS::HandleObject db) {
  uint32_t id=JS::GetReservedSlot(db,DbId).toInt32();
  for(auto& c:s.store->connections)if(c->id==id)return c.get();
  return nullptr;
}
bool StoreNames(JSContext* cx,unsigned n,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(n,vp);JS::RootedObject self(cx);
  State* s=Receiver(cx,args,&kDatabaseClass,&self);if(!s)return false;
  auto* connection=FindConnection(*s,self);std::vector<std::u16string> names;
  if(!connection || !s->store->model.Stores(connection->key,&names))return Throw(*s,"InvalidStateError");
  JS::RootedObject array(cx,JS::NewArrayObject(cx,0));JS::RootedValue value(cx);
  if(!array)return false;
  for(uint32_t i=0;i<names.size();++i)
    if(!String(cx,names[i],&value) || !JS_SetElement(cx,array,i,value))return false;
  JS::RootedObject list(cx,s->environment.StringList(array));if(!list)return false;
  args.rval().setObject(*list);return true;
}
bool DatabaseMethod(JSContext* cx,unsigned n,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(n,vp);JS::RootedObject self(cx);
  State* s=Receiver(cx,args,&kDatabaseClass,&self);if(!s)return false;
  auto* c=FindConnection(*s,self);
  if(!c || !s->store->model.Active(c->id) || JS::GetReservedSlot(self,9).toBoolean())
    return Throw(*s,"InvalidStateError");
  if(!n)return WebKit::Fail(cx,"IndexedDB store name is required");
  std::u16string name,path;int op=WebKit::Slot(args);JS::RootedValue input(cx,args[0]);
  // transaction(storeNames): a string or a sequence, deduplicated and sorted
  // (the scope is a set; objectStoreNames lists it in sorted order).
  std::vector<std::u16string> scope;
  if(op && input.isObject()) {
    JS::RootedObject names(cx,&input.toObject());bool array=false;uint32_t length;
    if(!JS::IsArrayObject(cx,names,&array))return false;
    if(!array)return WebKit::Fail(cx,"IndexedDB store names must be a string or array");
    if(!JS::GetArrayLength(cx,names,&length))return false;
    if(!length)return Throw(*s,"InvalidAccessError");
    JS::RootedValue item(cx);
    for(uint32_t i=0;i<length;++i) {
      std::u16string one;
      if(!JS_GetElement(cx,names,i,&item) || !Text(cx,item,&one))return false;
      scope.push_back(std::move(one));
    }
    std::sort(scope.begin(),scope.end());scope.erase(std::unique(scope.begin(),scope.end()),scope.end());
    name=scope.front();
  } else if(!Text(cx,input,&name))return false;
  if(op && scope.empty())scope.push_back(name);
  if(!op) {
    if(!JS::GetReservedSlot(self,8).toBoolean())return Throw(*s,"InvalidStateError");
    bool autoIncrement=false,inlinePath=false;
    if(args.get(1).isObject()) {
      JS::RootedObject options(cx,&args[1].toObject());JS::RootedValue value(cx);
      if(!JS_GetProperty(cx,options,"autoIncrement",&value))return false;
      autoIncrement=JS::ToBoolean(value);
      if(!JS_GetProperty(cx,options,"keyPath",&value))return false;
      if(!value.isNullOrUndefined()) { if(!Text(cx,value,&path))return false;inlinePath=true; }
      if(path.find(u'.')!=std::u16string::npos)return Throw(*s,"NotSupportedError");
    }
    // A key generator cannot fill the empty key path (the value itself).
    if(autoIncrement && inlinePath && path.empty())return Throw(*s,"InvalidAccessError");
    if(!s->store->model.CreateStore(c->key,name,path,autoIncrement))return Throw(*s,"ConstraintError");
    JS::RootedObject tx(cx),store(cx,NewObjectStore(*s,tx,name,path,self));
    if(!store)return false;
    // The upgrade transaction's objectStore(name) returns this same object.
    JS::RootedValue upgrade(cx,JS::GetReservedSlot(self,DbUpgrade));
    if(upgrade.isObject()) {
      JS::RootedObject owner(cx,&upgrade.toObject());
      if(!TransactionStore(*s,owner,name,path,self,store))return false;
    }
    args.rval().setObject(*store);return true;
  }
  std::u16string mode=u"readonly";
  if(!args.get(1).isUndefined() && !Text(cx,args[1],&mode))return false;
  if(mode!=u"readonly" && mode!=u"readwrite")return WebKit::Fail(cx,"Invalid IndexedDB transaction mode");
  for(const auto& one:scope)
    if(!s->store->model.Path(c->key,one,&path))return Throw(*s,"NotFoundError");
  JS::RootedObject tx(cx,NewTransaction(*s,self,scope,mode==u"readwrite"));
  if(!tx)return false;
  args.rval().setObject(*tx);return true;
}
}
