#include "web_indexeddb/keys.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/Wrapper.h"

namespace ruyijs::web_indexeddb {
const JSClass kKeyRangeClass={"IDBKeyRange",JSCLASS_HAS_RESERVED_SLOTS(6)};
namespace {
bool RangeReceiver(JSContext* cx,const JS::CallArgs& args,const JSClass* clasp,JS::MutableHandleObject out) {
  JS::HandleValue value=args.thisv();
  out.set(value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr);
  return (out && JS::GetClass(out)==clasp) || WebKit::FailBrand(cx,args,clasp->name);
}
bool EncodedBound(JSContext* cx,JS::HandleObject object,int slot,EncodedKey* out) {
  JS::RootedString text(cx,JS::GetReservedSlot(object,slot).toString());
  return KeyText(cx,text,out);
}
}
bool ReadRange(State& s,JS::HandleValue value,KeyRange* out,bool optional) {
  if(optional && value.isNullOrUndefined())return true;
  JS::RootedObject object(s.cx,value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr);
  if(object && JS::GetClass(object)==&kKeyRangeClass) {
    int flags=JS::GetReservedSlot(object,RangeFlags).toInt32();
    out->lowerOpen=flags&1;out->upperOpen=flags&2;
    return EncodedBound(s.cx,object,RangeLower,&out->lower) && EncodedBound(s.cx,object,RangeUpper,&out->upper);
  }
  if(!EncodeKey(s,value,&out->lower))return false;
  out->upper=out->lower;return true;
}
bool KeyRangeCreate(JSContext* cx,unsigned n,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(n,vp);auto& s=*static_cast<State*>(WebKit::Owner(args));
  const int op=WebKit::Slot(args);
  if(n<unsigned(op==3?2:1))return WebKit::Fail(cx,"IndexedDB range argument is required");
  KeyRange range;
  if(op==2) {
    range.lowerOpen=true;range.upperOpen=JS::ToBoolean(args.get(1));
    if(!EncodeKey(s,args[0],&range.upper))return false;
  } else {
    if(!EncodeKey(s,args[0],&range.lower))return false;
    if(op==0)range.upper=range.lower;
    else if(op==1){range.lowerOpen=JS::ToBoolean(args.get(1));range.upperOpen=true;}
    else {
      if(!EncodeKey(s,args[1],&range.upper))return false;
      range.lowerOpen=JS::ToBoolean(args.get(2));range.upperOpen=JS::ToBoolean(args.get(3));
      if(range.lower>range.upper || (range.lower==range.upper && (range.lowerOpen || range.upperOpen)))
        return Throw(s,"DataError");
    }
  }
  JS::RootedObject object(cx,s.kit.New(&kKeyRangeClass,s.keyRangeFace.prototype));
  JS::RootedValue lower(cx),upper(cx);
  if(!object || !String(cx,range.lower,&lower) || !String(cx,range.upper,&upper))return false;
  JS::SetReservedSlot(object,RangeOwner,JS::PrivateValue(&s));
  JS::SetReservedSlot(object,RangeLower,lower);JS::SetReservedSlot(object,RangeUpper,upper);
  JS::SetReservedSlot(object,RangeFlags,JS::Int32Value(int(range.lowerOpen)|int(range.upperOpen)*2));
  JS::SetReservedSlot(object,RangeLowerCache,JS::UndefinedValue());
  JS::SetReservedSlot(object,RangeUpperCache,JS::UndefinedValue());
  args.rval().setObject(*object);return true;
}
bool KeyRangeGet(JSContext* cx,unsigned n,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(n,vp);JS::RootedObject self(cx);
  auto& s=*static_cast<State*>(WebKit::Owner(args));
  if(!RangeReceiver(cx,args,&kKeyRangeClass,&self))return false;
  int slot=WebKit::Slot(args);
  if(slot>=2) {
    args.rval().setBoolean(JS::GetReservedSlot(self,RangeFlags).toInt32()&(slot==2?1:2));return true;
  }
  const int cache=slot==0?RangeLowerCache:RangeUpperCache;
  {JSAutoRealm realm(cx,self);args.rval().set(JS::GetReservedSlot(self,cache));}
  if(!JS_WrapValue(cx,args.rval()))return false;
  if(!args.rval().isUndefined())return true;
  EncodedKey key;
  if(!EncodedBound(cx,self,slot==0?RangeLower:RangeUpper,&key))return false;
  if(key.empty())return true;
  if(!DecodeKey(s,key,args.rval()))return false;
  {
    JSAutoRealm realm(cx,self);
    if(!JS_WrapValue(cx,args.rval()))return false;
    JS::SetReservedSlot(self,cache,args.rval());
  }
  return JS_WrapValue(cx,args.rval());
}
bool KeyRangeIncludes(JSContext* cx,unsigned n,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(n,vp);JS::RootedObject self(cx);
  if(!RangeReceiver(cx,args,&kKeyRangeClass,&self))return false;
  if(!n)return WebKit::Fail(cx,"includes requires a key");
  auto& s=*static_cast<State*>(WebKit::Owner(args));KeyRange range;EncodedKey key;
  if(!ReadRange(s,args.thisv(),&range,false) || !EncodeKey(s,args[0],&key))return false;
  args.rval().setBoolean(range.Contains(key));return true;
}
bool CompareKeys(JSContext* cx,unsigned n,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(n,vp);JS::RootedObject self(cx);
  if(!RangeReceiver(cx,args,&kFactoryClass,&self))return false;
  if(n<2)return WebKit::Fail(cx,"cmp requires two keys");
  auto& s=*static_cast<State*>(WebKit::Owner(args));EncodedKey a,b;
  if(!EncodeKey(s,args[0],&a) || !EncodeKey(s,args[1],&b))return false;
  args.rval().setInt32(a<b?-1:a>b?1:0);return true;
}
}
