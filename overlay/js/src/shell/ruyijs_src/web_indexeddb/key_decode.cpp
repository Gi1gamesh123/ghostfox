#include "web_indexeddb/keys.h"
#include "js/Array.h"
#include "js/ArrayBuffer.h"
#include "js/Date.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::web_indexeddb {
namespace {
namespace key = domain::indexed_database;
bool Decode(State& s,const EncodedKey& data,size_t& offset,unsigned depth,JS::MutableHandleValue out) {
  if(depth>=64 || offset>=data.size())return Throw(s,"DataError");
  JSContext* cx=s.cx;const char16_t type=data[offset++];
  if(type==key::NumberKey || type==key::DateKey) {
    if(data.size()-offset<8)return Throw(s,"DataError");
    const double number=key::KeyNumber(data,offset);offset+=8;
    if(type==key::NumberKey)out.setNumber(number);
    else {
      JS::RootedObject date(cx,JS::NewDateObject(cx,JS::TimeClip(number)));
      if(!date)return false;out.setObject(*date);
    }
    return true;
  }
  if(type==key::StringKey || type==key::BinaryKey) {
    std::u16string units;
    while(offset<data.size() && data[offset]) {
      if(data[offset++]!=1 || offset>=data.size())return Throw(s,"DataError");
      units.push_back(data[offset++]);
    }
    if(offset>=data.size())return Throw(s,"DataError");
    ++offset;
    if(type==key::StringKey)return String(cx,units,out);
    JS::RootedObject buffer(cx,JS::NewArrayBuffer(cx,units.size()));
    if(!buffer)return false;
    {
      JS::AutoCheckCannotGC noGC;bool shared;
      auto* bytes=JS::GetArrayBufferData(buffer,&shared,noGC);
      for(size_t i=0;i<units.size();++i)bytes[i]=uint8_t(units[i]);
    }
    out.setObject(*buffer);return true;
  }
  if(type!=key::ArrayKey)return Throw(s,"DataError");
  JS::RootedObject array(cx,JS::NewArrayObject(cx,0));
  if(!array)return false;
  uint32_t index=0;
  while(offset<data.size() && data[offset]) {
    JS::RootedValue value(cx);
    if(!Decode(s,data,offset,depth+1,&value) || !JS_SetElement(cx,array,index++,value))return false;
  }
  if(offset>=data.size())return Throw(s,"DataError");
  ++offset;out.setObject(*array);return true;
}
}
bool DecodeKey(State& s,const EncodedKey& key,JS::MutableHandleValue out) {
  size_t offset=0;
  return Decode(s,key,offset,0,out) && (offset==key.size() || Throw(s,"DataError"));
}
}
