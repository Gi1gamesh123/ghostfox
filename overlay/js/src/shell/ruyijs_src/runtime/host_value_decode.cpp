#include "runtime/host_values.h"
#include "runtime/host_bridge.h"
#include "runtime/typed_decode.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/String.h"
namespace ruyijs {
bool HostValues::Decode(JS::HandleValue encoded,JS::MutableHandleValue result,size_t depth) {
  if(depth>64||!Budget()||!encoded.isObject())return HostValueError(cx_,"Invalid host argument packet");
  JS::RootedObject packet(cx_,&encoded.toObject());
  JS::RootedValue body(cx_);std::string kind;
  if(!StringProperty(cx_,packet,"kind",&kind)||!JS_GetProperty(cx_,packet,"value",&body))return false;
  if(kind=="value"){
    if(!body.isObject())return HostValueError(cx_,"Host scalar requires a typed packet");
    JS::RootedObject scalar(cx_,&body.toObject());std::string type;
    if(!StringProperty(cx_,scalar,"type",&type))return false;
    if(type=="array"||type=="object")return HostValueError(cx_,"Host containers require recursive packets");
    JS::RootedValue data(cx_);
    if(!JS_GetProperty(cx_,scalar,"value",&data)||
       (data.isString()&&!Budget(JS_GetStringLength(data.toString()))))return false;
    return DecodeTyped(cx_,body,result);
  }
  if(kind=="reference"){
    uint64_t id;if(!HostValueIndex(cx_,body,&id))return false;
    auto* e=Find(id);if(!e)return false;
    result.set(e->value);return JS_WrapValue(cx_,result);
  }
  if(kind=="callback"){
    if(!body.isObject())return HostValueError(cx_,"Invalid host callback packet");
    JS::RootedObject info(cx_,&body.toObject()),function(cx_);
    JS::RootedValue key(cx_),name(cx_);
    if(!JS_GetProperty(cx_,info,"key",&key)||!JS_GetProperty(cx_,info,"name",&name)||
        !key.isString()||!name.isString())return HostValueError(cx_,"Invalid host callback identity");
    JS::RootedString id(cx_,key.toString()),label(cx_,name.toString());
    if(!JS_GetStringLength(id))return HostValueError(cx_,"Empty host callback identity");
    if(!Budget(JS_GetStringLength(id)+JS_GetStringLength(label)))return false;
    if(!bridge_.MakeCallback(id,label,&function))return false;
    result.setObject(*function);return true;
  }
  if(kind!="array"&&kind!="object")return HostValueError(cx_,"Unknown host argument kind");
  bool array=false;
  if(!body.isObject()||!JS::IsArrayObject(cx_,body,&array)||!array)return HostValueError(cx_,"Host arguments require an array");
  JS::RootedObject values(cx_,&body.toObject()),output(cx_,kind=="array"?JS::NewArrayObject(cx_,0):JS_NewPlainObject(cx_));
  uint32_t length;if(!output||!JS::GetArrayLength(cx_,values,&length))return false;
  if(length>10000)return HostValueError(cx_,"Host argument length limit exceeded");
  for(uint32_t i=0;i<length;++i){
    JS::RootedValue item(cx_),value(cx_);
    if(!JS_GetElement(cx_,values,i,&item))return false;
    if(kind=="array"){
      if(!Decode(item,&value,depth+1)||!JS_DefineElement(cx_,output,i,value,JSPROP_ENUMERATE))return false;
    }else{
      if(!item.isObject())return HostValueError(cx_,"Invalid host object pair");
      JS::RootedObject pair(cx_,&item.toObject());JS::RootedValue key(cx_),entry(cx_);JS::RootedId id(cx_);
      if(!JS_GetElement(cx_,pair,0,&key)||!key.isString()||!JS_GetElement(cx_,pair,1,&entry))
        return HostValueError(cx_,"Invalid host object key");
      JS::RootedString name(cx_,key.toString());
      if(!Budget(JS_GetStringLength(name))||!JS_StringToId(cx_,name,&id)||!Decode(entry,&value,depth+1)||
          !JS_DefinePropertyById(cx_,output,id,value,JSPROP_ENUMERATE))return false;
    }
  }
  result.setObject(*output);return true;
}
}
