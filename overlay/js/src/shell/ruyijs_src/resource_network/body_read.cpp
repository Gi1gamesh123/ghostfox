// Original FIFO completion consumes genuine Uint8Array chunks without JS results.
#include "resource_network/body_acquire_internal.h"
#include "resource_network/model_body_bytes.h"
#include "resource_network/state.h"
#include "jsfriendapi.h"
#include "js/Promise.h"
#include "js/Realm.h"
#include "js/experimental/TypedData.h"

namespace ruyijs::resource_network {
namespace {
bool WrongChunk(JSContext* cx,JS::HandleObject operation) {
  Error(cx,"Can't convert value to Uint8Array while consuming Body");
  JS::RootedValue reason(cx);
  if (!JS_GetPendingException(cx,&reason)) return BodyAcquisitionFailure(cx,operation);
  JS_ClearPendingException(cx);
  return FinishBodyAcquisition(cx,operation,BodyOutcome::WrongType,reason);
}
bool Chunk(JSContext* cx,JS::HandleObject,JS::HandleObject operation,JS::HandleValue value) {
  if (BodyTerminal(operation)) return true;
  JS::RootedObject chunk(cx,value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr);
  if (!chunk || !JS_IsUint8Array(chunk)) return WrongChunk(cx,operation);
  JS::RootedObject holder(cx,SlotObject(operation,OHolder));
  Status status;
  {
    JS::AutoCheckCannotGC nogc;
    bool shared=false;
    auto* data=static_cast<const uint8_t*>(JS_GetArrayBufferViewData(chunk,&shared,nogc));
    status=AppendBodyBytes(&HolderBody(holder)->bytes,data,JS_GetTypedArrayByteLength(chunk));
  }
  return (Report(cx,status) && QueueBodyRead(cx,operation)) || BodyAcquisitionFailure(cx,operation);
}
bool Close(JSContext* cx,JS::HandleObject,JS::HandleObject operation,JS::HandleValue) {
  return FinishBodyAcquisition(cx,operation,BodyOutcome::Complete);
}
bool SourceError(JSContext* cx,JS::HandleObject,JS::HandleObject operation,JS::HandleValue) {
  return FinishBodyAcquisition(cx,operation,BodyOutcome::SourceFailure);
}
const ReadCompletionSteps kSteps={Chunk,Close,SourceError};
}
bool ReadBodyTask(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject operation(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
  args.rval().setUndefined();
  if (BodyTerminal(operation)) return true;
  JSAutoRealm realm(cx,operation);
  JS::RootedObject holder(cx,SlotObject(operation,OHolder));
  if (HolderBody(holder)->source!=BodySource::ScriptStream)
    return FinishBodyAcquisition(cx,operation,BodyOutcome::Complete);
  JS::RootedObject reader(cx,SlotObject(operation,OReader)),factories(cx,SlotObject(operation,OFactories)),target(cx);
  State* state=StreamFactoryState(cx,factories);
  if (!state || !NewReadCompletion(cx,operation,operation,kSteps,&target)) return BodyAcquisitionFailure(cx,operation);
  JS::SetReservedSlot(operation,OPhase,JS::Int32Value(int(BodyPhase::Waiting)));
  return ReadDefaultReaderInto(*state,reader,target) || BodyAcquisitionFailure(cx,operation);
}
}
