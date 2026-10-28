#pragma once
#include "execution_scheduling/state.h"
#include "js/Object.h"
namespace ruyijs::execution_scheduling {
enum SchedulerSlot { SFactory,SSlots };
enum TaskFactorySlot { TState,TPage,TScheduler,TControllerProto,TSignalProto,TControllerMap,TSignalMap,
 TAbortCtor,TAbortGetSignal,TAbortCall,TAbortAny,TAborted,TReason,TAdd,TRemove,TException,TEvents,TEventProto,TArrayValues,TFSlots };
enum TaskDataSlot { TFactory,TSignal,TPriority,THandler,TChanging,TDependents,TPrioritySource,TListener,TFixed,TDSlots };
extern const JSClass kSchedulerClass,kTaskFactoryClass,kTaskDataClass;
bool InstallScheduler(State&,JS::HandleObject,PageHelpers&,interaction::Realm&);
bool SchedulerMethod(JSContext*,unsigned,JS::Value*);
bool SchedulerGlobal(JSContext*,unsigned,JS::Value*);
bool TaskControllerConstruct(JSContext*,unsigned,JS::Value*);
bool TaskSetPriority(JSContext*,unsigned,JS::Value*);
bool TaskSignalMember(JSContext*,unsigned,JS::Value*);
bool TaskSignalAny(JSContext*,unsigned,JS::Value*);
bool TaskRead(JSContext*,JS::HandleObject,const char*,JS::MutableHandleValue);
bool TaskPriority(JSContext*,JS::HandleValue,int*);
bool TaskFind(JSContext*,JS::HandleValue,bool,JS::MutableHandleObject);
bool TaskInit(JSContext*,JS::HandleObject,JS::HandleObject,int,JS::MutableHandleObject);
bool TaskChange(JSContext*,JS::HandleObject,int);
bool TaskException(JSContext*,JS::HandleObject,const char*);
bool TaskListen(JSContext*,JS::HandleObject,JS::HandleValue,const char*,JS::HandleValue,bool);
bool TaskAborted(JSContext*,JS::HandleObject,JS::HandleValue,bool*,JS::MutableHandleValue);
bool InvokeWebTask(State&,Callback&,JS::MutableHandleValue);
bool ScheduleWebTask(JSContext*,JS::HandleObject,const JS::CallArgs&,bool,JS::HandleObject);
bool WebTaskAbort(JSContext*,unsigned,JS::Value*);
inline constexpr const char* kTaskPriorities[]={"user-blocking","user-visible","background"};
inline State& TaskState(JS::HandleObject f){return *static_cast<State*>(JS::GetReservedSlot(f,TState).toPrivate());}
JSObject* TaskFunction(JSContext*,JS::HandleObject,JSNative,const char*,unsigned,int,unsigned=0);
}
