#include "runtime/host_values.h"
#include <new>
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/ArrayBuffer.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/String.h"
#include "js/Symbol.h"
#include "js/Wrapper.h"
#include "js/experimental/TypedData.h"
namespace ruyijs {
bool HostValues::Label(JS::HandleObject object,JS::MutableHandleValue result,bool functionSource) {
  JS::RootedValue constructor(cx_),name(cx_),tag(cx_);
  const bool callable=JS::IsCallable(object);
  if(functionSource&&callable){
    if(!JS_GetProperty(cx_,object,"name",&name))return false;
  }else if(!callable){
    JS::RootedId tagKey(cx_,JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx_,JS::SymbolCode::toStringTag)));
    if(!JS_GetPropertyById(cx_,object,tagKey,&tag))return false;
    JS::RootedObject proto(cx_);
    if(!JS_GetPrototype(cx_,object,&proto))return false;
    if(proto&&!JS_GetProperty(cx_,proto,"constructor",&constructor))return false;
    if(constructor.isObject()){
      JS::RootedObject ctor(cx_,&constructor.toObject());
      if(!JS_GetProperty(cx_,ctor,"name",&name))return false;
    }
  }
  if(tag.isString())name.set(tag);
  JS::RootedString label(cx_,name.isString()?name.toString():JS_NewStringCopyZ(cx_,callable?"Function":"Object"));
  if(!label||!Budget(JS_GetStringLength(label)))return false;
  JS::RootedString prefix(cx_,JS_NewStringCopyZ(cx_,functionSource&&callable
      ?(JS_GetStringLength(label)?"function ":"function"):"[object "));
  JS::RootedString suffix(cx_,JS_NewStringCopyZ(cx_,functionSource&&callable?"() { ... }":"]"));
  if(!prefix||!suffix)return false;
  prefix=JS_ConcatStrings(cx_,prefix,label);if(!prefix)return false;
  label=JS_ConcatStrings(cx_,prefix,suffix);if(!label)return false;
  result.setString(label);return true;
}
bool HostValues::Bytes(JS::HandleObject object,JS::MutableHandleValue result) {
  const bool view=JS_IsArrayBufferViewObject(object);
  const size_t length=view?JS_GetArrayBufferViewByteLength(object):JS::GetArrayBufferByteLength(object);
  if(!Budget(length*2))return false;
  auto chars=std::unique_ptr<char[]>(new(std::nothrow) char[length*2]);
  if(!chars){JS_ReportOutOfMemory(cx_);return false;}
  {
    JS::AutoCheckCannotGC noGC;bool shared=false;
    const uint8_t* data=view?static_cast<uint8_t*>(JS_GetArrayBufferViewData(object,&shared,noGC))
                            :JS::GetArrayBufferData(object,&shared,noGC);
    if(shared)return HostValueError(cx_,"Shared host byte views require a snapshot");
    static const char digits[]="0123456789abcdef";
    for(size_t i=0;i<length;++i){chars[i*2]=digits[data[i]>>4];chars[i*2+1]=digits[data[i]&15];}
  }
  JS::RootedString text(cx_,JS_NewStringCopyN(cx_,chars.get(),length*2));
  if(!text)return false;
  JS::RootedValue value(cx_,JS::StringValue(text));return HostValuePacket(cx_,"bytes",value,result);
}
bool HostValues::Visit(JS::HandleValue input,bool toPython,JS::HandleValue receiver,JS::MutableHandleValue result,size_t depth) {
  if(depth>64||!Budget())return HostValueError(cx_,"Host value nesting limit exceeded");
  JS::RootedValue value(cx_,input);
  if(value.isUndefined()||value.isBigInt())value.setNull();
  if(value.isSymbol()){
    JS::RootedSymbol symbol(cx_,value.toSymbol());
    JS::RootedString text(cx_,JS::GetSymbolDescription(symbol));
    if(!text)text=JS_GetEmptyString(cx_);
    JS::RootedString prefix(cx_,JS_NewStringCopyZ(cx_,"Symbol(")),suffix(cx_,JS_NewStringCopyZ(cx_,")"));
    if(!prefix||!suffix)return false;
    text=JS_ConcatStrings(cx_,prefix,text);if(!text)return false;
    text=JS_ConcatStrings(cx_,text,suffix);if(!text)return false;value.setString(text);
  }
  if(!value.isObject()){
    if(value.isString()&&!Budget(JS_GetStringLength(value.toString())))return false;
    return HostValuePacket(cx_,"scalar",value,result);
  }
  JS::RootedObject object(cx_,js::CheckedUnwrapStatic(&value.toObject()));
  if(!object)return HostValueError(cx_,"Host object access denied");
  JSAutoRealm realm(cx_,object);
  value.setObject(*object);
  if(JS::IsArrayBufferObject(object)||JS_IsArrayBufferViewObject(object))return Bytes(object,result);
  bool array=false;if(!JS::IsArrayObject(cx_,object,&array))return false;
  JS::RootedObject proto(cx_);
  if(!JS_GetPrototype(cx_,object,&proto))return false;
  const bool plain=proto==JS::GetRealmObjectPrototype(cx_)&&std::string(JS::GetClass(object)->name)=="Object";
  if(!array&&!plain){
    if(toPython){if(!Label(object,&value))return false;return HostValuePacket(cx_,"scalar",value,result);}
    JS::RootedValue bound(cx_,receiver);if(!JS_WrapValue(cx_,&bound))return false;
    return Retain(value,bound,result);
  }
  JS::RootedObject values(cx_,JS::NewArrayObject(cx_,0));if(!values)return false;
  uint32_t length=0;
  JS::RootedIdVector keys(cx_);
  if(array){if(!JS::GetArrayLength(cx_,object,&length))return false;}
  else {if(!js::GetPropertyKeys(cx_,object,0,&keys))return false;length=uint32_t(keys.length());}
  if(length>10000)return HostValueError(cx_,"Host container length limit exceeded");
  for(uint32_t i=0;i<length;++i){
    JS::RootedValue child(cx_),encoded(cx_);
    if(array){if(!JS_GetElement(cx_,object,i,&child)||!Visit(child,false,JS::UndefinedHandleValue,&encoded,depth+1))return false;}
    else {
      JS::RootedId id(cx_,keys[i]);JS::RootedValue key(cx_);
      if(!JS_IdToValue(cx_,id,&key))return false;
      JS::RootedString text(cx_,JS::ToString(cx_,key));if(!text||!Budget(JS_GetStringLength(text)))return false;
      if(!JS_GetPropertyById(cx_,object,id,&child)||!Visit(child,false,JS::UndefinedHandleValue,&encoded,depth+1)||!JS_WrapValue(cx_,&encoded))return false;
      JS::RootedObject pair(cx_,JS::NewArrayObject(cx_,2));
      if(!pair||!JS_DefineElement(cx_,pair,0,text,JSPROP_ENUMERATE)||!JS_DefineElement(cx_,pair,1,encoded,JSPROP_ENUMERATE))return false;
      encoded.setObject(*pair);
    }
    if(!JS_WrapValue(cx_,&encoded)||!JS_DefineElement(cx_,values,i,encoded,JSPROP_ENUMERATE))return false;
  }
  value.setObject(*values);return HostValuePacket(cx_,array?"array":"object",value,result);
}
}
