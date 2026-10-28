#include "runtime/host_values.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include "js/ValueArray.h"
namespace ruyijs {
bool HostValues::Request(JS::HandleObject request,JS::MutableHandleValue result) {
  std::string action;if(!StringProperty(cx_,request,"action",&action))return false;
  JS::RootedValue index(cx_),raw(cx_);uint64_t id=0;
  if(action=="release"){
    if(!JS_GetProperty(cx_,request,"references",&raw)||!raw.isObject())return HostValueError(cx_,"Invalid host releases");
    JS::RootedObject list(cx_,&raw.toObject());bool array=false;uint32_t length;
    if(!JS::IsArrayObject(cx_,list,&array)||!array||!JS::GetArrayLength(cx_,list,&length)||length>100000)
      return HostValueError(cx_,"Invalid host release list");
    for(uint32_t i=0;i<length;++i){if(!JS_GetElement(cx_,list,i,&index)||!HostValueIndex(cx_,index,&id))return false;}
    for(uint32_t i=0;i<length;++i){if(!JS_GetElement(cx_,list,i,&index)||!HostValueIndex(cx_,index,&id))return false;Release(id);}
    result.setUndefined();return true;
  }
  if(action=="count"){result.setNumber(double(count_));return true;}
  if(!JS_GetProperty(cx_,request,"reference",&index)||!HostValueIndex(cx_,index,&id))return false;
  auto* entry=Find(id);if(!entry)return false;
  JS::RootedValue value(cx_,entry->value),receiver(cx_,entry->receiver),localRequest(cx_,JS::ObjectValue(*request));
  JS::RootedObject object(cx_,js::CheckedUnwrapStatic(&value.toObject()));
  if(!object)return HostValueError(cx_,"Host object access denied");
  {
    JSAutoRealm realm(cx_,object);
    if(!JS_WrapValue(cx_,&localRequest)||!JS_WrapValue(cx_,&receiver))return false;
    JS::RootedObject input(cx_,&localRequest.toObject());value.setObject(*object);
    if(action=="to_py"||action=="str"){
      if(!Label(object,&value,action=="str")||!HostValuePacket(cx_,"scalar",value,result))return false;
    }else if(action=="keys"){
      JS::RootedIdVector keys(cx_);
      if(!js::GetPropertyKeys(cx_,object,JSITER_OWNONLY,&keys))return false;
      if(keys.length()>10000)return HostValueError(cx_,"Host keys length limit exceeded");
      JS::RootedObject array(cx_,JS::NewArrayObject(cx_,0));if(!array)return false;
      for(size_t i=0;i<keys.length();++i){JS::RootedId key(cx_,keys[i]);JS::RootedValue name(cx_);
        if(!JS_IdToValue(cx_,key,&name))return false;
        if(!JS_DefineElement(cx_,array,uint32_t(i),name,JSPROP_ENUMERATE))return false;}
      value.setObject(*array);if(!Visit(value,false,JS::UndefinedHandleValue,result,0))return false;
    }else if(action=="call"){
      JS::RootedValue arguments(cx_),decoded(cx_);
      if(!JS_GetProperty(cx_,input,"arguments",&arguments)||!Decode(arguments,&decoded)||!decoded.isObject())return false;
      JS::RootedObject array(cx_,&decoded.toObject());bool isArray=false;uint32_t length;
      if(!JS::IsArrayObject(cx_,array,&isArray)||!isArray||!JS::GetArrayLength(cx_,array,&length))
        return HostValueError(cx_,"Host call arguments require an array");
      JS::RootedVector<JS::Value> argv(cx_);
      if(!argv.resize(length)){JS_ReportOutOfMemory(cx_);return false;}
      for(uint32_t i=0;i<length;++i)if(!JS_GetElement(cx_,array,i,argv[i]))return false;
      if(receiver.isUndefined())receiver.setObject(*page_);
      if(!JS_WrapValue(cx_,&receiver)||!JS::Call(cx_,receiver,value,argv,&raw)||
          !Visit(raw,false,JS::UndefinedHandleValue,result,0))return false;
    }else{
      JS::RootedValue name(cx_);JS::RootedId key(cx_);
      if(!JS_GetProperty(cx_,input,"key",&name)||!name.isString())return HostValueError(cx_,"Host property key must be a string");
      JS::RootedString text(cx_,name.toString());if(!JS_StringToId(cx_,text,&key))return false;
      bool present;if(!JS_HasPropertyById(cx_,object,key,&present))return false;
      if(action=="has"){raw.setBoolean(present);if(!HostValuePacket(cx_,"scalar",raw,result))return false;}
      else if(action=="get"){
        if(!present){raw.setNull();if(!HostValuePacket(cx_,"missing",raw,result))return false;}
        else if(!JS_GetPropertyById(cx_,object,key,&raw)||!Visit(raw,false,value,result,0))return false;
      }else if(action=="set"){
        JS::RootedValue encoded(cx_);if(!JS_GetProperty(cx_,input,"value",&encoded)||!Decode(encoded,&raw)||
            !JS_SetPropertyById(cx_,object,key,raw))return false;
        raw.setNull();if(!HostValuePacket(cx_,"scalar",raw,result))return false;
      }else if(action=="delete"){
        JS::ObjectOpResult removed;if(!JS_DeletePropertyById(cx_,object,key,removed)||!removed.checkStrict(cx_,object,key))return false;
        raw.setNull();if(!HostValuePacket(cx_,"scalar",raw,result))return false;
      }else return HostValueError(cx_,"Unknown host value operation");
    }
  }
  return JS_WrapValue(cx_,result);
}
}
