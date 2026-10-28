#pragma once
#include "resource_network/body_acquire.h"
#include "resource_network/readable_stream.h"

namespace ruyijs::resource_network {
enum BodyOperationSlot { OHolder,OReader,OTarget,OFactories,OFinish,OPhase,OLifetime,OSlots };
enum class BodyPhase { Preparing,Queued,Waiting,Terminal };
extern const JSClass kBodyOperationClass;
inline bool BodyTerminal(JSObject* operation) {
  return JS::GetReservedSlot(operation,OPhase).toInt32()==int(BodyPhase::Terminal);
}
bool AttachBodyOperation(State&,JS::HandleObject);
void UnlinkBodyOperation(JSObject*);
bool FinishBodyAcquisition(JSContext*,JS::HandleObject,BodyOutcome,JS::HandleValue reason=JS::UndefinedHandleValue);
bool BodyAcquisitionFailure(JSContext*,JS::HandleObject);
bool QueueBodyRead(JSContext*,JS::HandleObject);
bool ReadBodyTask(JSContext*,unsigned,JS::Value*);
}
