#pragma once
#include "execution_scheduling/state.h"
#include "js/Object.h"
namespace ruyijs::execution_scheduling {
enum IdleFactorySlot { IState,IPage,IPrototype,INext,IFactorySlots };
enum DeadlineSlot { DState,DTimeout,DEnd,DeadlineSlots };
extern const JSClass kIdleFactoryClass,kIdleDeadlineClass;
bool InstallIdle(State&,JS::HandleObject,PageHelpers&);
bool IdleRequest(JSContext*,unsigned,JS::Value*);
bool IdleCancel(JSContext*,unsigned,JS::Value*);
bool IdleIllegalConstructor(JSContext*,unsigned,JS::Value*);
bool IdleMember(JSContext*,unsigned,JS::Value*);
bool InvokeIdle(State&,Callback&,const TaskSnapshot&,JS::MutableHandleValue);
JSObject* IdleFunction(JSContext*,JS::HandleObject,JSNative,const char*,unsigned,int,unsigned=0);
inline State& IdleState(JS::HandleObject factory){return *static_cast<State*>(JS::GetReservedSlot(factory,IState).toPrivate());}
}
