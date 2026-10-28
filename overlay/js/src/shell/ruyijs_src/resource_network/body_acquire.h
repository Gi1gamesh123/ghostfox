#pragma once
#include "resource_network/body.h"

namespace ruyijs::resource_network {
enum class BodyAcquireMode { PublicConsume, AfterValidation };
enum class BodyOutcome { Complete, WrongType, SourceFailure, Retired, Unavailable, Locked, NativeFailure };
using BodyFinish=bool (*)(JSContext*,JS::HandleObject target,JS::HandleObject holder,
                          BodyOutcome,JS::HandleValue reason);
// The immutable native table must have static storage, like ReadCompletionSteps.
struct BodyAcquireSteps { BodyFinish finish; };
bool AcquireBody(JSContext*,JS::HandleObject holder,BodyAcquireMode,
                   JS::HandleObject target,const BodyAcquireSteps&);
enum class BodyResultKind { Text,Json,ArrayBuffer,Blob,FormData,Bytes };
bool ConsumeStreamBody(JSContext*,const JS::CallArgs&,BodyResultKind);
void DetachBodyOperations(State&);
void RetireBodyConsumers(State&);
}
