#include "web_indexeddb/keys.h"
#include "js/Array.h"
#include "js/ArrayBuffer.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/StableStringChars.h"
#include "js/Wrapper.h"
#include "js/experimental/TypedData.h"
#include "jsfriendapi.h"
#include <cmath>

namespace ruyijs::web_indexeddb {
namespace key = domain::indexed_database;
bool KeyText(JSContext* cx,JS::HandleString text,std::u16string* out) {
  JS::AutoStableStringChars chars(cx);
  if(!chars.initTwoByte(cx,text))return false;
  out->assign(chars.twoByteChars(),chars.length());return true;
}
bool KeyType(JSContext* cx,JS::HandleValue value,bool* valid) {
  *valid=value.isNumber() || value.isString();
  if(*valid || !value.isObject())return true;
  JS::RootedObject object(cx,&value.toObject());js::ESClass type;
  if(!JS::GetBuiltinClass(cx,object,&type))return false;
  *valid=type==js::ESClass::Date || type==js::ESClass::Array ||
         JS::IsArrayBufferObject(object) || JS_IsArrayBufferViewObject(object);
  return true;
}
namespace {
bool Encode(State& s,JS::HandleValue value,EncodedKey& out,unsigned depth) {
  JSContext* cx=s.cx;
  if(depth>=64 || out.size()>key::kKeyUnitLimit)return Throw(s,"DataError");
  if(value.isNumber()) {
    if(std::isnan(value.toNumber()))return Throw(s,"DataError");
    key::AppendNumberKey(out,value.toNumber(),key::NumberKey);return true;
  }
  if(value.isString()) {
    JS::RootedString text(cx,value.toString());std::u16string chars;
    if(JS_GetStringLength(text)>key::kKeyUnitLimit/2)return Throw(s,"DataError");
    if(!KeyText(cx,text,&chars))return false;
    key::AppendStringKey(out,chars);return true;
  }
  if(!value.isObject())return Throw(s,"DataError");
  JS::RootedObject object(cx,&value.toObject());js::ESClass type;
  if(!JS::GetBuiltinClass(cx,object,&type))return false;
  if(type==js::ESClass::Date) {
    double time;
    if(!js::DateGetMsecSinceEpoch(cx,object,&time))return false;
    if(std::isnan(time))return Throw(s,"DataError");
    key::AppendNumberKey(out,time,key::DateKey);return true;
  }
  if(JS::IsArrayBufferObject(object) || JS_IsArrayBufferViewObject(object)) {
    JS::RootedObject raw(cx,js::CheckedUnwrapStatic(object));
    const bool view=JS_IsArrayBufferViewObject(raw);bool shared=false;
    JS::RootedObject buffer(cx,view?JS_GetArrayBufferViewBuffer(cx,raw,&shared):raw.get());
    if(!buffer)return false;
    if(shared || JS::IsDetachedArrayBufferObject(buffer))return Throw(s,"DataError");
    size_t length=view?JS_GetArrayBufferViewByteLength(raw):JS::GetArrayBufferByteLength(buffer);
    if(length>key::kKeyUnitLimit/2)return Throw(s,"DataError");
    out.push_back(key::BinaryKey);
    {
      JS::AutoCheckCannotGC noGC;
      const auto* data=view?static_cast<const uint8_t*>(JS_GetArrayBufferViewData(raw,&shared,noGC)):
                            JS::GetArrayBufferData(buffer,&shared,noGC);
      for(size_t i=0;i<length;++i){out.push_back(1);out.push_back(data[i]);}
    }
    out.push_back(0);return true;
  }
  if(type!=js::ESClass::Array)return Throw(s,"DataError");
  uint32_t length;
  if(!JS::GetArrayLength(cx,object,&length))return false;
  out.push_back(key::ArrayKey);
  for(uint32_t i=0;i<length;++i) {
    JS::RootedId id(cx);bool own;
    if(!JS_IndexToId(cx,i,&id) || !JS_HasOwnPropertyById(cx,object,id,&own))return false;
    if(!own)return Throw(s,"DataError");
    JS::RootedValue item(cx);
    if(!JS_GetElement(cx,object,i,&item) || !Encode(s,item,out,depth+1))return false;
  }
  out.push_back(0);return true;
}
}
bool EncodeKey(State& s,JS::HandleValue value,EncodedKey* out) {
  out->clear();
  return Encode(s,value,*out,0) && (out->size()<=key::kKeyUnitLimit || Throw(s,"DataError"));
}
}
