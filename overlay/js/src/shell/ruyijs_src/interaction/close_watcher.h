#pragma once
#include "interaction/internal.h"
#include "js/Object.h"
#include "js/Wrapper.h"

namespace ruyijs::interaction {
enum CloseSlot { CLifetime,CManager,CGroup,CPrev,CNext,CRunning,COnCancel,COnClose,CSlots };
enum CloseManagerSlot { MHead,MTail,MAllowed,MNextAllows,MCount,MSlots };
enum CloseGroupSlot { GPrev,GNext,GHead,GTail,GSlots };
enum CloseFactorySlot { FCtor,FProto,FCancelInvoker,FCloseInvoker,FAdd,FRemove,FSlots };
struct CloseLifetime { State* owner; void* target=nullptr; CloseLifetime* prev=nullptr; CloseLifetime* next=nullptr; };
extern const JSClass kCloseWatcherClass;
inline JSObject* CloseObject(JSObject* object,unsigned slot) {
  const auto value=JS::GetReservedSlot(object,slot);
  return value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr;
}
inline bool CloseCanGrow(JSObject* manager) {
  return JS::GetReservedSlot(manager,MCount).toNumber()<JS::GetReservedSlot(manager,MAllowed).toNumber();
}
State* CloseOwner(JSObject*);
JSObject* CloseReceiver(JSContext*,JS::HandleValue,const char*);
bool CloseFullyActive(State&);
bool CloseIsActive(State&,JSObject*);
bool AttachCloseWatcher(State&,JS::HandleObject);
void DetachCloseWatchers(State&);
bool NewCloseManager(JSContext*,JS::MutableHandleObject);
bool PrepareCloseGroup(JSContext*,JS::HandleObject,JS::MutableHandleObject);
void PublishCloseWatcher(JSObject*,JSObject*,JSObject*);
void RemoveCloseWatcher(JSObject*);
void ClearCloseManager(JSObject*);
bool RequestCloseWatcher(State&,JS::HandleObject,bool,bool*);
bool CloseWatcherNow(State&,JS::HandleObject);
bool ProcessCloseManager(State&);
bool InstallCloseWatcher(State&);
bool CloseWatcherConstruct(JSContext*,unsigned,JS::Value*);
bool CloseRequestCall(JSContext*,unsigned,JS::Value*);
bool CloseCloseCall(JSContext*,unsigned,JS::Value*);
bool CloseDestroyCall(JSContext*,unsigned,JS::Value*);
bool CloseCancelGet(JSContext*,unsigned,JS::Value*);
bool CloseCancelSet(JSContext*,unsigned,JS::Value*);
bool CloseCloseGet(JSContext*,unsigned,JS::Value*);
bool CloseCloseSet(JSContext*,unsigned,JS::Value*);
bool CloseHandlerInvoke(JSContext*,unsigned,JS::Value*);
bool FireTargetEvent(State&,JS::HandleObject,const char*,bool,bool,bool* = nullptr);
bool ProcessCloseInput(State&,JS::HandleObject,JS::HandleObject,const char*,bool);
}
