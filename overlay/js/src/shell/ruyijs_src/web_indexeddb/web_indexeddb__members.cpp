#include "web_indexeddb/state.h"
#include "js/CallAndConstruct.h"
#include "js/Object.h"
#include "js/String.h"
namespace ruyijs::web_indexeddb {
bool RequestGet(JSContext* cx,unsigned n,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(n,vp);JS::RootedObject self(cx);
  State* s=Receiver(cx,args,&kRequestClass,&self);if (!s) return false;
  int op=WebKit::Slot(args);bool ready=JS::GetReservedSlot(self,ReadySlot).toBoolean();
  if (op<2) {
    if (!ready) return Throw(*s,"InvalidStateError");
    args.rval().set(JS::GetReservedSlot(self,op==0?ResultSlot:ErrorSlot));
  } else if (op==2) {
    JSString* str=JS_NewStringCopyZ(cx,ready?"done":"pending");
    if (!str) return false;
    args.rval().setString(str);
  } else args.rval().set(JS::GetReservedSlot(self,op==3?SourceSlot:TransactionSlot));
  return true;
}
bool DatabaseGet(JSContext* cx,unsigned n,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(n,vp);JS::RootedObject self(cx);
  if (!Receiver(cx,args,&kDatabaseClass,&self)) return false;
  args.rval().set(JS::GetReservedSlot(self,WebKit::Slot(args)));return true;
}
bool Close(JSContext* cx,unsigned n,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(n,vp);JS::RootedObject self(cx);
  State* s=Receiver(cx,args,&kDatabaseClass,&self);if (!s) return false;
  uint32_t id=JS::GetReservedSlot(self,DbId).toInt32();
  JS::SetReservedSlot(self,9,JS::BooleanValue(true));
  if (!FinishClose(*s,self)) return false;
  args.rval().setUndefined();return true;
}
namespace {
const char* const names[]={"success","error","blocked","upgradeneeded","abort","close","error","versionchange","complete","abort","error"};
bool Handler(JSContext* cx,unsigned n,JS::Value* vp,bool set) {
  auto args=JS::CallArgsFromVp(n,vp);JS::RootedObject self(cx);
  int op=WebKit::Slot(args);bool database=op>=4;
  State* s=Receiver(cx,args,op>=8?&kTransactionClass:(database?&kDatabaseClass:&kRequestClass),&self,
                    op==2||op==3?"IDBOpenDBRequest":nullptr);
  if (!s) return false;
  int slot=HandlerSlot+(op%4);
  JS::RootedValue previous(cx,JS::GetReservedSlot(self,slot));
  if (!set) {args.rval().set(previous);return true;}
  JS::RootedValue next(cx,args.get(0)),ignored(cx),receiver(cx,JS::ObjectValue(*self));
  if (!next.isObject() || !JS::IsCallable(&next.toObject())) next.setNull();
  JS::RootedString type(cx,JS_NewStringCopyZ(cx,names[op]));
  if (!type) return false;
  JS::RootedValueArray<2> values(cx);values[0].setString(type);
  if (previous.isObject()) {
    values[1].set(previous);
    if (!JS::Call(cx,receiver,s->removeListener,values,&ignored)) return false;
  }
  if (next.isObject()) {
    values[1].set(next);
    if (!JS::Call(cx,receiver,s->addListener,values,&ignored)) return false;
  }
  JS::SetReservedSlot(self,slot,next);args.rval().setUndefined();return true;
}
}
bool HandlerGet(JSContext* c,unsigned n,JS::Value* v) {return Handler(c,n,v,false);}
bool HandlerSet(JSContext* c,unsigned n,JS::Value* v) {return Handler(c,n,v,true);}
}
