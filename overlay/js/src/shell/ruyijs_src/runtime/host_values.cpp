#include "runtime/host_values.h"
#include <cmath>
#include <new>
#include "jsapi.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/String.h"
namespace ruyijs {
bool HostValueError(JSContext* cx,const char* message) {
  JS_ReportErrorASCII(cx,"%s",message);return false;
}
bool HostValuePacket(JSContext* cx,const char* kind,JS::HandleValue value,JS::MutableHandleValue result) {
  JS::RootedObject packet(cx,JS::NewArrayObject(cx,2));
  JS::RootedString tag(cx,JS_NewStringCopyZ(cx,kind));
  if(!packet||!tag||!JS_DefineElement(cx,packet,0,tag,JSPROP_ENUMERATE)||!JS_DefineElement(cx,packet,1,value,JSPROP_ENUMERATE))return false;
  result.setObject(*packet);return true;
}
bool HostValueIndex(JSContext* cx,JS::HandleValue v,uint64_t* id) {
  if(!v.isNumber()||!std::isfinite(v.toNumber())||v.toNumber()<1||v.toNumber()>9007199254740991.0||std::trunc(v.toNumber())!=v.toNumber())
    return HostValueError(cx,"Invalid host reference identity");
  *id=uint64_t(v.toNumber());return true;
}
bool HostValues::Budget(size_t units) {
  units_+=units;
  return (++nodes_<=10000&&units_<=1024*1024)||HostValueError(cx_,"Host value traversal budget exceeded");
}
HostValues::Entry* HostValues::Find(uint64_t id) {
  for(auto* e=entries_.get();e;e=e->next.get())if(e->id==id)return e;
  HostValueError(cx_,"Host reference is released or belongs to another context");return nullptr;
}
void HostValues::Release(uint64_t id) {
  auto* p=&entries_;
  while(*p){if((*p)->id==id){auto old=std::move(*p);*p=std::move(old->next);--count_;return;}p=&(*p)->next;}
}
void HostValues::Finish(bool success) {
  if(success)return;
  while(entries_&&entries_->id>=checkpoint_){auto old=std::move(entries_);entries_=std::move(old->next);--count_;}
}
bool HostValues::Retain(JS::HandleValue v,JS::HandleValue receiver,JS::MutableHandleValue result) {
  if(count_>=100000||next_>=9007199254740991ULL)return HostValueError(cx_,"Host reference limit exceeded");
  std::unique_ptr<Entry> entry(new(std::nothrow) Entry(cx_,next_++,v,receiver));
  if(!entry){JS_ReportOutOfMemory(cx_);return false;}
  JS::RootedValue id(cx_,JS::DoubleValue(double(entry->id)));
  if(!HostValuePacket(cx_,JS::IsCallable(&v.toObject())?"function":"reference",id,result))return false;
  entry->next=std::move(entries_);entries_=std::move(entry);++count_;return true;
}
bool HostValues::Encode(JS::HandleValue value,bool toPython,JS::MutableHandleValue result,JS::HandleValue receiver) {
  {
    JSAutoRealm realm(cx_,page_);
    JS::RootedValue input(cx_,value),localReceiver(cx_,receiver);
    if(!JS_WrapValue(cx_,&input)||!JS_WrapValue(cx_,&localReceiver)||!Visit(input,toPython,localReceiver,result,0))return false;
  }
  return JS_WrapValue(cx_,result);
}
}
