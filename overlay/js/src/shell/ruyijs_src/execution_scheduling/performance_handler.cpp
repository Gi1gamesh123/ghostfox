#include "execution_scheduling/performance.h"
#include "js/CallAndConstruct.h"
namespace ruyijs::execution_scheduling {
bool PerfSetHandler(JSContext* cx,JS::HandleObject self,JS::HandleValue input) {
 JSAutoRealm realm(cx,self);
 JS::RootedValue next(cx,input),previous(cx,JS::GetReservedSlot(self,PHandler));
 if(!JS_WrapValue(cx,&next))return false;
 if(!next.isObject()||!JS::IsCallable(&next.toObject()))next.setNull();
 if(next==previous)return true;
 JS::RootedString name(cx,JS_NewStringCopyZ(cx,"resourcetimingbufferfull"));
 JS::RootedValueArray<2> args(cx);JS::RootedValue result(cx),fn(cx);
 if(!name)return false;args[0].setString(name);
 if(previous.isObject()){
  fn=JS::GetReservedSlot(self,PRemoveListener);args[1].set(previous);
  if(!JS::Call(cx,self,fn,args,&result))return false;
 }
 if(next.isObject()){
  fn=JS::GetReservedSlot(self,PAddListener);args[1].set(next);
  if(!JS::Call(cx,self,fn,args,&result))return false;
 }
 JS::SetReservedSlot(self,PHandler,next);return true;
}
}
