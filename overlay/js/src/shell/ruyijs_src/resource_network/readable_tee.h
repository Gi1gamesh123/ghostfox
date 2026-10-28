#pragma once
#include "resource_network/readable_stream.h"
#include "resource_network/native_source.h"

namespace ruyijs::resource_network {
enum TeeSlot { TSource,TReader,TBranch1,TBranch2,TCancelPromise,TReason1,TReason2,
               TFactories,TFlags,TSlots };
enum TeeFlag { TReading=1,TReadAgain1=2,TReadAgain2=4,TCanceled1=8,TCanceled2=16 };
extern const JSClass kTeeOperationClass;
extern const NativeSourceSteps kTeeSourceSteps;
inline int TeeFlags(JSObject* operation) { return JS::GetReservedSlot(operation,TFlags).toInt32(); }
inline void SetTeeFlags(JSObject* operation,int flags) { JS::SetReservedSlot(operation,TFlags,JS::Int32Value(flags)); }
inline bool TeeCanceled(JSObject* operation,unsigned branch) {
  return (TeeFlags(operation)&(branch?TCanceled2:TCanceled1))!=0;
}
CallbackResult TeeRecoverFailure(JSContext*,JS::HandleObject operation,CallbackResult);
bool TeeOperationFailure(JSContext*,JS::HandleObject operation);
bool TeeCompleteMicrotask(JSContext*,bool success);
bool TeePull(JSContext*,JS::HandleObject operation,unsigned branch);
bool TeePullDefault(JSContext*,JS::HandleObject operation);
bool TeePullByte(JSContext*,JS::HandleObject operation,unsigned branch);
bool TeeObserveReader(JSContext*,JS::HandleObject operation,JS::HandleObject reader);
bool TeeFinishCancel(JSContext*,JS::HandleObject operation);
bool TeeErrorBranches(JSContext*,JS::HandleObject operation,JS::HandleValue reason);
bool TeeCloseBranches(JSContext*,JS::HandleObject operation);
bool TeeQueueChunk(JSContext*,JS::HandleObject operation,JS::HandleValue chunk,JSNative task);
bool StreamTee(JSContext*,unsigned,JS::Value*);
bool TeeReadable(JSContext*,JS::HandleObject source,JS::MutableHandleObject first,
                   JS::MutableHandleObject second,JS::MutableHandleObject operation);
}
