// One native AbortSignal state, shared by Request, Controller and TaskSignal.
#include "resource_network/state.h"
#include "resource_network/abort.h"
#include "interaction/realm.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Exception.h"
#include "js/Wrapper.h"
namespace ruyijs::resource_network {
bool UnwrapAbort(JS::HandleValue value,JS::MutableHandleObject out){
 out.set(value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr);
 return out&&JS::GetClass(out)==&kAbortSignalClass&&!JS::GetReservedSlot(out,AOwner).isUndefined();
}
bool IsAbortSignal(JS::HandleValue value){
 JSObject* o=value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr;
 return o&&JS::GetClass(o)==&kAbortSignalClass&&!JS::GetReservedSlot(o,AOwner).isUndefined();
}
bool AbortSignalled(JS::HandleValue value){
 return IsAbortSignal(value)&&JS::GetReservedSlot(js::CheckedUnwrapStatic(&value.toObject()),AAborted).toBoolean();
}
bool NewAbortSignal(State& s,JS::MutableHandleObject result){
 JSAutoRealm realm(s.cx,s.page);
 JS::RootedObject object(s.cx,JS_NewObjectWithGivenProto(s.cx,&kAbortSignalClass,s.signalPrototype));
 JS::RootedObject dependents(s.cx,JS::NewArrayObject(s.cx,0)),sources(s.cx,JS::NewArrayObject(s.cx,0));
 if(!object||!dependents||!sources)return false;
 JS::SetReservedSlot(object,AAborted,JS::BooleanValue(false));JS::SetReservedSlot(object,AReason,JS::UndefinedValue());
 JS::SetReservedSlot(object,AOwner,JS::PrivateValue(&s));JS::SetReservedSlot(object,AHandler,JS::NullValue());
 JS::SetReservedSlot(object,ADependents,JS::ObjectValue(*dependents));JS::SetReservedSlot(object,ASources,JS::ObjectValue(*sources));
 if(!AttachAbortSignal(s,object))return false;result.set(object);return true;
}
bool AbortException(State& s,const char* name,JS::MutableHandleValue result,const char* message){
 JS::RootedValue ctor(s.cx,s.abortException);JS::RootedString text(s.cx,JS_NewStringCopyZ(s.cx,name));
 JS::RootedString detail(s.cx,message?JS_NewStringCopyZ(s.cx,message):text.get());
 JS::RootedValueArray<2> args(s.cx);JS::RootedObject error(s.cx);
 if(!text||!detail||!JS_WrapValue(s.cx,&ctor))return false;args[0].setString(detail);args[1].setString(text);
 if(!JS::Construct(s.cx,ctor,args,&error))return false;result.setObject(*error);return true;
}
bool AbortDefaultReason(State& s,JS::MutableHandleValue result){return AbortException(s,"AbortError",result);}
bool AbortReason(State& s,JS::HandleValue value,JS::MutableHandleValue result){
 JS::RootedObject object(s.cx);if(!UnwrapAbort(value,&object))return Error(s.cx,"Invalid AbortSignal");
 result.set(JS::GetReservedSlot(object,AReason));return JS_WrapValue(s.cx,result);
}
bool AbortSignalAbort(State& s,JS::HandleObject input,JS::HandleValue reason){
 JSContext* cx=s.cx;JS::RootedValue val(cx,JS::ObjectValue(*input));JS::RootedObject object(cx);
 if(!UnwrapAbort(val,&object))return Error(cx,"Invalid AbortSignal");
 if(JS::GetReservedSlot(object,AAborted).toBoolean())return true;
 JS::RootedObjectVector pending(cx),notify(cx);if(!pending.append(object))return false;
 // Set every dependent state before user handlers can observe the transition.
 for(size_t i=0;i<pending.length();++i){
  JS::RootedObject target(cx,pending[i]);JSAutoRealm realm(cx,target);
  if(JS::GetReservedSlot(target,AAborted).toBoolean())continue;
  JS::RootedValue local(cx,reason);if(!JS_WrapValue(cx,&local))return false;
  JS::SetReservedSlot(target,AAborted,JS::BooleanValue(true));JS::SetReservedSlot(target,AReason,local);
  if(!notify.append(target))return false;
  JS::RootedObject list(cx,&JS::GetReservedSlot(target,ADependents).toObject());uint32_t n=0;
  if(!JS::GetArrayLength(cx,list,&n))return false;
  for(uint32_t j=0;j<n;++j){JS::RootedValue value(cx);JS::RootedObject child(cx);
   if(!JS_GetElement(cx,list,j,&value)||!UnwrapAbort(value,&child)||!pending.append(child))return false;}
  if(!JS::SetArrayLength(cx,list,0))return false;
 }
 for(size_t i=0;i<notify.length();++i){JS::RootedObject target(cx,notify[i]);JSAutoRealm realm(cx,target);
  auto& owner=*static_cast<State*>(JS::GetReservedSlot(target,AOwner).toPrivate());
  if(!RunAbortAlgorithms(cx,target)||!owner.events->Fire(target,"abort",false,false))return false;}
 return true;
}
}
