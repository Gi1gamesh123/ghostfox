#include "web_indexeddb/keys.h"
#include "js/PropertyAndElement.h"
namespace ruyijs::web_indexeddb {
bool InstallRecords(State& s) {
  JS::RootedObject parent(s.cx,s.events.prototype()),none(s.cx);
  if(!s.kit.Define("IDBTransaction",&kTransactionClass,parent,WebKit::Reject,&s.transactionFace) ||
     !s.kit.Define("IDBObjectStore",&kStoreClass,none,WebKit::Reject,&s.storeFace))return false;
  if(!s.kit.Accessor(s.databaseFace,"IDBDatabase","objectStoreNames",StoreNames,nullptr,0) ||
     !s.kit.Method(s.databaseFace,"IDBDatabase","createObjectStore",DatabaseMethod,1,0) ||
     !s.kit.Method(s.databaseFace,"IDBDatabase","transaction",DatabaseMethod,1,1))return false;
  // Firefox IDBTransaction member order: methods, attributes, event handlers.
  if(!s.kit.Method(s.transactionFace,"IDBTransaction","objectStore",TransactionMethod,1,0) ||
     !s.kit.Method(s.transactionFace,"IDBTransaction","commit",TransactionMethod,0,2) ||
     !s.kit.Method(s.transactionFace,"IDBTransaction","abort",TransactionMethod,0,1))return false;
  const char* txFields[]={"objectStoreNames","mode","durability","db","error"};
  const int slots[]={TxStores,TxMode,TxDurability,TxDatabase,TxError};
  for(int i=0;i<5;++i)
    if(!s.kit.Accessor(s.transactionFace,"IDBTransaction",txFields[i],TransactionGet,nullptr,slots[i]))return false;
  // Handler ids index the event names (members.cpp): 8 complete, 9 abort, 10 error.
  const char* handlers[]={"onabort","oncomplete","onerror"};const int ids[]={9,8,10};
  for(int i=0;i<3;++i)
    if(!s.kit.Accessor(s.transactionFace,"IDBTransaction",handlers[i],HandlerGet,HandlerSet,ids[i]))return false;
  // Firefox IDBObjectStore member order: requests, cursors, index management, attributes.
  struct Method { const char* name; unsigned length; int kind; JSNative native; };
  const Method methods[]={{"put",1,Put,StoreMethod},{"add",1,Add,StoreMethod},{"delete",1,21,StoreMember},
    {"clear",0,22,StoreMember},{"get",1,Get,StoreMethod},{"getKey",1,GetKey,StoreMethod},
    {"getAll",0,GetAll,StoreMethod},{"mozGetAll",0,GetAll,StoreMethod},{"getAllKeys",0,GetAllKeys,StoreMethod},
    {"getAllRecords",0,GetAllRecords,StoreMethod},{"count",0,Count,StoreMethod},
    {"openCursor",0,26,StoreMember},{"openKeyCursor",0,27,StoreMember},{"createIndex",2,23,StoreMember},
    {"index",1,24,StoreMember},{"deleteIndex",1,25,StoreMember}};
  if(!JS_DeleteProperty(s.cx,s.storeFace.prototype,"constructor"))return false;
  for(const auto& method:methods)
    if(!s.kit.Method(s.storeFace,"IDBObjectStore",method.name,method.native,method.length,method.kind))return false;
  if(!s.kit.Accessor(s.storeFace,"IDBObjectStore","name",StoreGet,StoreSetName,StoreName) ||
     !s.kit.Accessor(s.storeFace,"IDBObjectStore","keyPath",StoreGet,nullptr,StoreKeyPath) ||
     !s.kit.Accessor(s.storeFace,"IDBObjectStore","indexNames",StoreMember,nullptr,28) ||
     !s.kit.Accessor(s.storeFace,"IDBObjectStore","transaction",StoreGet,nullptr,StoreTransaction) ||
     !s.kit.Accessor(s.storeFace,"IDBObjectStore","autoIncrement",StoreMember,nullptr,29) ||
     !JS_DefineProperty(s.cx,s.storeFace.prototype,"constructor",s.storeFace.constructor,0) ||
     !InstallIndexes(s))return false;
  return InstallKeys(s);
}
}
