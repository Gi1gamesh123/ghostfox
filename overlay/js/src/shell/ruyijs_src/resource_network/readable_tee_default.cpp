// Default tee receives direct read outcomes and schedules one original microtask.
#include "resource_network/readable_tee.h"
#include "resource_network/readable_controller.h"
#include "resource_network/read_completion.h"
#include "resource_network/state.h"
#include "jsfriendapi.h"
#include "js/Promise.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
namespace {
bool ChunkTask(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject operation(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
  JSAutoRealm realm(cx,operation);
  JS::RootedValue chunk(cx,js::GetFunctionNativeReserved(&args.callee(),1));
  args.rval().setUndefined();
  SetTeeFlags(operation,TeeFlags(operation)&~TReadAgain1);
  for (unsigned i=0;i<2;i++) {
    JS::RootedObject branch(cx,SlotObject(operation,i?TBranch2:TBranch1));
    JS::RootedObject controller(cx,SlotObject(branch,SController));
    if (!TeeCanceled(operation,i) && ReadableStatus(branch)==StreamStatus::Readable &&
        !(DefaultFlags(controller)&DCloseRequested) && !DefaultEnqueue(cx,controller,chunk))
      return TeeCompleteMicrotask(cx,TeeOperationFailure(cx,operation));
  }
  SetTeeFlags(operation,TeeFlags(operation)&~TReading);
  return TeeCompleteMicrotask(cx,!(TeeFlags(operation)&TReadAgain1) || TeePull(cx,operation,0));
}
bool Chunk(JSContext* cx,JS::HandleObject,JS::HandleObject operation,JS::HandleValue value) {
  return TeeQueueChunk(cx,operation,value,ChunkTask) || TeeOperationFailure(cx,operation);
}
bool Close(JSContext* cx,JS::HandleObject,JS::HandleObject operation,JS::HandleValue) {
  SetTeeFlags(operation,TeeFlags(operation)&~TReading);
  return TeeCloseBranches(cx,operation) || TeeOperationFailure(cx,operation);
}
bool Fail(JSContext*,JS::HandleObject,JS::HandleObject operation,JS::HandleValue) {
  SetTeeFlags(operation,TeeFlags(operation)&~TReading);return true;
}
const ReadCompletionSteps kDefaultTeeSteps={Chunk,Close,Fail};
}
bool TeePullDefault(JSContext* cx,JS::HandleObject operation) {
  if (TeeFlags(operation)&TReading) {
    SetTeeFlags(operation,TeeFlags(operation)|TReadAgain1);return true;
  }
  JS::RootedObject factories(cx,SlotObject(operation,TFactories));
  State* state=StreamFactoryState(cx,factories);
  JS::RootedObject reader(cx,SlotObject(operation,TReader)),target(cx);
  if (!state || !NewReadCompletion(cx,factories,operation,kDefaultTeeSteps,&target))
    return TeeOperationFailure(cx,operation);
  SetTeeFlags(operation,TeeFlags(operation)|TReading);
  if (ReadDefaultReaderInto(*state,reader,target)) return true;
  return (TeeFlags(operation)&TReading)?TeeOperationFailure(cx,operation):false;
}
bool TeeQueueChunk(JSContext* cx,JS::HandleObject operation,JS::HandleValue chunk,JSNative task) {
  JSAutoRealm realm(cx,operation);
  JS::RootedObject trigger(cx,JS::NewPromiseObject(cx,nullptr));
  JS::RootedFunction callback(cx,js::NewFunctionWithReserved(cx,task,0,0,nullptr));
  JS::RootedValue stored(cx,chunk);
  if (!trigger || !callback || !JS_WrapValue(cx,&stored)) return false;
  JS::RootedObject handler(cx,JS_GetFunctionObject(callback));
  js::SetFunctionNativeReserved(handler,0,JS::ObjectValue(*operation));
  js::SetFunctionNativeReserved(handler,1,stored);
  return JS::AddPromiseReactions(cx,trigger,handler,handler) &&
      JS::ResolvePromise(cx,trigger,JS::UndefinedHandleValue);
}
}
