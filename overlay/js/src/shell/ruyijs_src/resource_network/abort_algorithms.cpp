#include "resource_network/state.h"
#include "resource_network/abort.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
namespace ruyijs::resource_network {
bool AddAbortAlgorithm(JSContext* cx,JS::HandleValue value,JS::HandleValue callback){
 JS::RootedObject signal(cx);if(!UnwrapAbort(value,&signal))return Error(cx,"Illegal AbortSignal");
 JSAutoRealm realm(cx,signal);JS::RootedValue local(cx,callback);if(!JS_WrapValue(cx,&local))return false;
 JS::RootedObject list(cx);auto slot=JS::GetReservedSlot(signal,AAlgorithms);
 if(slot.isObject())list=&slot.toObject();else{list=JS::NewArrayObject(cx,0);if(!list)return false;
  JS::SetReservedSlot(signal,AAlgorithms,JS::ObjectValue(*list));}
 uint32_t n=0;return JS::GetArrayLength(cx,list,&n)&&JS_DefineElement(cx,list,n,local,JSPROP_ENUMERATE);
}
bool RemoveAbortAlgorithm(JSContext* cx,JS::HandleValue value,JS::HandleValue callback){
 JS::RootedObject signal(cx);if(!UnwrapAbort(value,&signal))return Error(cx,"Illegal AbortSignal");
 JSAutoRealm realm(cx,signal);JS::RootedValue local(cx,callback);if(!JS_WrapValue(cx,&local))return false;
 auto slot=JS::GetReservedSlot(signal,AAlgorithms);if(!slot.isObject())return true;
 JS::RootedObject list(cx,&slot.toObject());uint32_t n=0;if(!JS::GetArrayLength(cx,list,&n))return false;
 for(uint32_t i=0;i<n;++i){JS::RootedValue entry(cx);if(!JS_GetElement(cx,list,i,&entry))return false;
  if(entry==local)return JS_DefineElement(cx,list,i,JS::UndefinedHandleValue,JSPROP_ENUMERATE);}
 return true;
}
bool RunAbortAlgorithms(JSContext* cx,JS::HandleObject signal){
 auto slot=JS::GetReservedSlot(signal,AAlgorithms);if(!slot.isObject())return true;
 JS::RootedObject list(cx,&slot.toObject());JS::SetReservedSlot(signal,AAlgorithms,JS::UndefinedValue());
 uint32_t n=0;if(!JS::GetArrayLength(cx,list,&n))return false;
 for(uint32_t i=0;i<n;++i){JS::RootedValue fn(cx),ignored(cx);if(!JS_GetElement(cx,list,i,&fn))return false;
  if(fn.isObject()&&!JS::Call(cx,JS::UndefinedHandleValue,fn,JS::HandleValueArray::empty(),&ignored))return false;}
 return true;
}
}
