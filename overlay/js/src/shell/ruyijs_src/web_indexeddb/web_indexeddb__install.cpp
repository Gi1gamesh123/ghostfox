#include "web_indexeddb/realm.h"
#include "web_indexeddb/state.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
namespace ruyijs::web_indexeddb {
const JSClass kFactoryClass = {"IDBFactory",JSCLASS_HAS_RESERVED_SLOTS(1)};
const JSClass kRequestClass = {"IDBRequest",JSCLASS_HAS_RESERVED_SLOTS(10)};
const JSClass kDatabaseClass = {"IDBDatabase",JSCLASS_HAS_RESERVED_SLOTS(DbSlots)};
const JSClass kTransactionClass = {"IDBTransaction",JSCLASS_HAS_RESERVED_SLOTS(TxSlots)};
const JSClass kStoreClass = {"IDBObjectStore",JSCLASS_HAS_RESERVED_SLOTS(StoreSlots)};
namespace {
bool Illegal(JSContext* cx,unsigned n,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(n,vp);NativeConstructorTrace trace(cx,args);
  return trace.Finish(WebKit::Fail(cx,"Illegal IndexedDB constructor"));
}
bool Handlers(State& s,WebFace& f,const char* iface,const char* const* names,int count,int offset) {
  for (int i=0;i<count;++i)
    if (!s.kit.Accessor(f,iface,names[i],HandlerGet,HandlerSet,offset+i)) return false;
  return true;
}
}
Realm::Realm(JSContext* c,JS::HandleObject p,PageHelpers& h,interaction::Realm& e,
             browser_environment::Environment& b,Realm* prior)
    : state_(std::make_unique<State>(c,p,h,e,b,prior?prior->state_.get():nullptr)) {}
Realm::~Realm()=default;
void Realm::SetTaskPort(bool (*port)(void*,JS::HandleObject),void* context) {
  state_->store->post=port;state_->store->context=context;
}
bool Realm::AdoptStore() { return Adopt(*state_); }
bool Realm::Install() { return InstallOn(state_->kit.page,"Window"); }
bool Realm::InstallWorker(JS::HandleObject globalScopePrototype) {
  return InstallOn(globalScopePrototype,"WorkerGlobalScope");
}
bool Realm::InstallOn(JS::HandleObject holder,const char* iface) {
  auto& s=*state_;JSAutoRealm realm(s.cx,s.kit.page);
  JS::RootedObject parent(s.cx,s.events.prototype()),none(s.cx);
  if (!s.kit.Define("IDBFactory",&kFactoryClass,none,Illegal,&s.factoryFace) ||
      !s.kit.Define("IDBRequest",&kRequestClass,parent,Illegal,&s.requestFace) ||
      !s.kit.Define("IDBOpenDBRequest",&kRequestClass,s.requestFace.prototype,Illegal,&s.openFace) ||
      !s.kit.Define("IDBDatabase",&kDatabaseClass,parent,Illegal,&s.databaseFace)) return false;
  if (!JS_SetPrototype(s.cx,s.openFace.constructor,s.requestFace.constructor)) return false;
  const char* methods[]={"open","deleteDatabase","databases"};
  for (int i=0;i<3;++i)
    if (!s.kit.Method(s.factoryFace,"IDBFactory",methods[i],FactoryMethod,i==2?0:1,i)) return false;
  const char* fields[]={"result","error","readyState","source","transaction"};
  for (int i=0;i<5;++i)
    if (!s.kit.Accessor(s.requestFace,"IDBRequest",fields[i],RequestGet,nullptr,i)) return false;
  const char* requestHandlers[]={"onsuccess","onerror"};
  const char* openHandlers[]={"onblocked","onupgradeneeded"};
  const char* dbHandlers[]={"onabort","onclose","onerror","onversionchange"};
  if (!Handlers(s,s.requestFace,"IDBRequest",requestHandlers,2,0) ||
      !Handlers(s,s.openFace,"IDBOpenDBRequest",openHandlers,2,2) ||
      !Handlers(s,s.databaseFace,"IDBDatabase",dbHandlers,4,4) ||
      !s.kit.Accessor(s.databaseFace,"IDBDatabase","name",DatabaseGet,nullptr,DbName) ||
      !s.kit.Accessor(s.databaseFace,"IDBDatabase","version",DatabaseGet,nullptr,DbVersion) ||
      !s.kit.Method(s.databaseFace,"IDBDatabase","close",Close,0,0)) return false;
  s.factory=s.kit.New(&kFactoryClass,s.factoryFace.prototype);
  if (!s.factory) return false;
  JS::SetReservedSlot(s.factory,0,JS::PrivateValue(&s));
  JS::RootedObject getter(s.cx,s.kit.Function(FactoryGet,"get indexedDB",0,0));
  if (!getter || !JS_DefineProperty(s.cx,holder,"indexedDB",getter,none,JSPROP_ENUMERATE) ||
      !s.kit.helpers->Accessor(iface,holder,"indexedDB") ||
      !JS_GetProperty(s.cx,s.kit.page,"Event",&s.eventCtor) ||
      !JS_GetProperty(s.cx,s.kit.page,"DOMException",&s.exceptionCtor) ||
      !JS_GetProperty(s.cx,parent,"addEventListener",&s.addListener) ||
      !JS_GetProperty(s.cx,parent,"removeEventListener",&s.removeListener)) return false;
  if (!s.parent) {
    s.store->anchor=&s;
    s.pump=s.kit.Function(Pump,"IndexedDB task",0,0);
    if (!s.pump) return false;
  }
  return InstallRecords(s);
}
}
