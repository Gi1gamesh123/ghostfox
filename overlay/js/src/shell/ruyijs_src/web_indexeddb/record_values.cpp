#include "web_indexeddb/keys.h"
#include "js/ArrayBuffer.h"
#include "js/Object.h"
#include "js/Wrapper.h"
#include <cstring>
namespace ruyijs::web_indexeddb {
const JSClass kRecordClass={"IDBRecord",JSCLASS_HAS_RESERVED_SLOTS(8)};
// Slot 7: the primary key text when it differs from the key (index records).
JSObject* NewRecord(State& s,const Record& record,const std::u16string* primary) {
  JSContext* cx=s.cx;
  JS::RootedObject object(cx,s.kit.New(&kRecordClass,s.recordFace.prototype));
  JS::RootedValue key(cx);JS::RootedObject bytes(cx,JS::NewArrayBuffer(cx,record.second.size()));
  if(!object || !bytes || !String(cx,record.first,&key))return nullptr;
  {
    JS::AutoCheckCannotGC noGC;bool shared;
    auto* data=JS::GetArrayBufferData(bytes,&shared,noGC);
    if(!record.second.empty())std::memcpy(data,record.second.data(),record.second.size());
  }
  JS::SetReservedSlot(object,0,JS::PrivateValue(&s));JS::SetReservedSlot(object,1,key);
  JS::SetReservedSlot(object,2,JS::ObjectValue(*bytes));
  for(int i=3;i<6;++i)JS::SetReservedSlot(object,i,JS::UndefinedValue());
  JS::SetReservedSlot(object,6,JS::Int32Value(0));
  JS::RootedValue primaryText(cx);
  if(primary && !String(cx,*primary,&primaryText))return nullptr;
  JS::SetReservedSlot(object,7,primaryText);
  return object;
}
bool RecordGet(JSContext* cx,unsigned n,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(n,vp);
  JS::RootedObject object(cx,args.thisv().isObject()?js::CheckedUnwrapStatic(&args.thisv().toObject()):nullptr);
  if(!object || JS::GetClass(object)!=&kRecordClass)return WebKit::FailBrand(cx,args,"IDBRecord");
  int field=WebKit::Slot(args),flags=JS::GetReservedSlot(object,6).toInt32();
  if(flags&(1<<field)) {
    {JSAutoRealm realm(cx,object);args.rval().set(JS::GetReservedSlot(object,field+3));}
    return JS_WrapValue(cx,args.rval());
  }
  State& s=*static_cast<State*>(WebKit::Owner(args));
  if(field<2) {
    const JS::Value& source=field==1 && JS::GetReservedSlot(object,7).isString() ? JS::GetReservedSlot(object,7)
                                                                                 : JS::GetReservedSlot(object,1);
    JS::RootedString text(cx,source.toString());EncodedKey key;
    if(!KeyText(cx,text,&key) || !DecodeKey(s,key,args.rval()))return false;
  } else {
    JS::RootedObject buffer(cx,&JS::GetReservedSlot(object,2).toObject());
    std::vector<uint8_t> bytes(JS::GetArrayBufferByteLength(buffer));
    {
      JS::AutoCheckCannotGC noGC;bool shared;
      const auto* data=JS::GetArrayBufferData(buffer,&shared,noGC);
      if(!bytes.empty())std::memcpy(bytes.data(),data,bytes.size());
    }
    if(!CloneRead(s,bytes,args.rval()))return false;
  }
  {
    JSAutoRealm realm(cx,object);
    if(!JS_WrapValue(cx,args.rval()))return false;
    JS::SetReservedSlot(object,field+3,args.rval());
    JS::SetReservedSlot(object,6,JS::Int32Value(flags|(1<<field)));
  }
  return JS_WrapValue(cx,args.rval());
}
}
