#include "web_indexeddb/state.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include <cmath>
namespace ruyijs::web_indexeddb {
bool FactoryGet(JSContext* cx,unsigned n,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(n,vp);auto& s=*static_cast<State*>(WebKit::Owner(args));
  if (!args.thisv().isObject() || &args.thisv().toObject()!=s.kit.page)
    return WebKit::Fail(cx,"Window.indexedDB called on an incompatible receiver");
  args.rval().setObject(*s.factory);return true;
}
bool FactoryMethod(JSContext* cx,unsigned n,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(n,vp);JS::RootedObject self(cx);
  State* state=Receiver(cx,args,&kFactoryClass,&self);
  if (!state) return false;
  auto& s=*state;JSAutoRealm realm(cx,s.kit.page);int op=WebKit::Slot(args);
  Key key;
  if (op!=2 && (!n || !Text(cx,args[0],&key.second)))
    return n?false:WebKit::Fail(cx,"IndexedDB requires a database name");
  uint64_t version=0;
  if (op==0 && !args.get(1).isUndefined()) {
    double number;
    if (!JS::ToNumber(cx,args[1],&number)) return false;
    if (!std::isfinite(number) || number<1 || number>9007199254740991.0)
      return WebKit::Fail(cx,"IndexedDB version must be a positive exact integer");
    version=uint64_t(std::trunc(number));
  }
  if (!Origin(s,&key.first)) return false;
  if (op==2) {
    std::vector<Info> entries;
    if (!s.store->model.List(key.first,&entries)) {JS_ReportOutOfMemory(cx);return false;}
    JS::RootedObject array(cx,JS::NewArrayObject(cx,0));JS::RootedValue name(cx),item(cx);
    if (!array) return false;
    uint32_t index=0;
    for (const auto& row:entries) {
      JS::RootedObject object(cx,JS_NewPlainObject(cx));
      if (!object || !String(cx,row.name,&name) ||
          !JS_DefineProperty(cx,object,"name",name,JSPROP_ENUMERATE) ||
          !JS_DefineProperty(cx,object,"version",double(row.version),JSPROP_ENUMERATE)) return false;
      item.setObject(*object);
      if (!JS_SetElement(cx,array,index++,item)) return false;
    }
    item.setObject(*array);return s.kit.Resolve(item,args.rval());
  }
  if (s.store->jobs.size()>=1024) return Throw(s,"QuotaExceededError");
  JS::RootedObject request(cx,s.kit.New(&kRequestClass,s.openFace.prototype));
  if (!request || !s.events.Attach(request)) return false;
  JS::SetReservedSlot(request,OwnerSlot,JS::PrivateValue(&s));
  JS::SetReservedSlot(request,ResultSlot,JS::UndefinedValue());
  JS::SetReservedSlot(request,ErrorSlot,JS::NullValue());
  JS::SetReservedSlot(request,ReadySlot,JS::BooleanValue(false));
  JS::SetReservedSlot(request,SourceSlot,JS::NullValue());
  JS::SetReservedSlot(request,TransactionSlot,JS::NullValue());
  for (int i=0;i<4;++i) JS::SetReservedSlot(request,HandlerSlot+i,JS::NullValue());
  s.store->jobs.push_back(std::make_unique<Job>(&s,cx,request,std::move(key),version,op==1));
  if (!Schedule(*s.store)) {s.store->jobs.pop_back();return false;}
  args.rval().setObject(*request);return true;
}
}
