#include "execution_scheduling/idle.h"
#include "runtime/native_constructor_trace.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/Wrapper.h"
#include <algorithm>
namespace ruyijs::execution_scheduling {
const JSClass kIdleDeadlineClass={"IdleDeadline",JSCLASS_HAS_RESERVED_SLOTS(DeadlineSlots)};
bool IdleIllegalConstructor(JSContext* cx,unsigned argc,JS::Value* vp){
 auto args=JS::CallArgsFromVp(argc,vp);NativeConstructorTrace trace(cx,args);
 return Error(cx,"Illegal constructor.");
}
bool IdleMember(JSContext* cx,unsigned argc,JS::Value* vp){
 auto args=JS::CallArgsFromVp(argc,vp);
 JS::RootedObject self(cx,args.thisv().isObject()?js::CheckedUnwrapStatic(&args.thisv().toObject()):nullptr);
 if(!self||JS::GetClass(self)!=&kIdleDeadlineClass)return Error(cx,"Illegal IdleDeadline receiver");
 bool timeout=JS::GetReservedSlot(self,DTimeout).toBoolean();
 auto& s=*static_cast<State*>(JS::GetReservedSlot(self,DState).toPrivate());
 int op=js::GetFunctionNativeReserved(&args.callee(),1).toInt32();
 s.Observe("IdleDeadline",op?"didTimeout":"timeRemaining",int(op?ApiAccess::Read:ApiAccess::Call));
 if(op){args.rval().setBoolean(timeout);return true;}
 s.SyncClock();double remaining=JS::GetReservedSlot(self,DEnd).toNumber()-s.model.Clock().now;
 args.rval().setDouble(timeout?0:std::clamp(remaining,0.0,50.0));return true;
}
bool InvokeIdle(State& s,Callback& callback,const TaskSnapshot& task,JS::MutableHandleValue result){
 JSContext* cx=s.cx;JS::RootedObject factory(cx,&callback.arguments[0].toObject());
 JS::RootedObject page(cx,&JS::GetReservedSlot(factory,IPage).toObject());JSAutoRealm realm(cx,page);
 JS::RootedObject prototype(cx,&JS::GetReservedSlot(factory,IPrototype).toObject());
 JS::RootedObject deadline(cx,JS_NewObjectWithGivenProto(cx,&kIdleDeadlineClass,prototype));if(!deadline)return false;
 JS::SetReservedSlot(deadline,DState,JS::PrivateValue(&s));JS::SetReservedSlot(deadline,DTimeout,JS::BooleanValue(task.didTimeout));
 JS::SetReservedSlot(deadline,DEnd,JS::DoubleValue(task.idleUntil));
 JS::RootedValue argument(cx,JS::ObjectValue(*deadline)),handler(cx,callback.handler);
 if(!JS_WrapValue(cx,&handler))return false;
 return JS::Call(cx,JS::UndefinedHandleValue,handler,JS::HandleValueArray(argument),result);
}
}
