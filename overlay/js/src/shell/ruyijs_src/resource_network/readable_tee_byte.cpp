// Byte tee switches the original reader only when no native request is pending.
#include "resource_network/readable_tee_byte.h"
#include "resource_network/readable_byte.h"
#include "resource_network/state.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
namespace {
bool SwitchReader(JSContext* cx,JS::HandleObject operation,bool byob,JS::MutableHandleObject result) {
  result.set(SlotObject(operation,TReader));
  if (IsByobReader(result)==byob) return true;
  JS::RootedObject stream(cx,SlotObject(operation,TSource));
  if (SlotObject(stream,SHead)) return Error(cx,"Tee reader switch has a pending read");
  if (!ReleaseReader(cx,result)) return false;
  JS::RootedObject prototype(cx,SlotObject(stream,byob?SByobPrototype:SReaderPrototype));
  if (!NewReader(cx,stream,prototype,byob,result)) return false;
  JS::SetReservedSlot(operation,TReader,JS::ObjectValue(*result));
  return TeeObserveReader(cx,operation,result);
}
}
bool TeePullByte(JSContext* cx,JS::HandleObject operation,unsigned branch) {
  if (TeeFlags(operation)&TReading) {
    SetTeeFlags(operation,TeeFlags(operation)|(branch?TReadAgain2:TReadAgain1));return true;
  }
  JS::RootedObject targetBranch(cx,SlotObject(operation,branch?TBranch2:TBranch1));
  JS::RootedObject controller(cx,SlotObject(targetBranch,SController)),request(cx),reader(cx),target(cx);
  if (!GetByobRequest(cx,controller,&request) || !SwitchReader(cx,operation,bool(request),&reader))
    return TeeOperationFailure(cx,operation);
  JS::RootedObject factories(cx,SlotObject(operation,TFactories));
  State* state=StreamFactoryState(cx,factories);
  const auto& steps=request?(branch?kByteByob2TeeSteps:kByteByob1TeeSteps):kByteDefaultTeeSteps;
  if (!state || !NewReadCompletion(cx,factories,operation,steps,&target))
    return TeeOperationFailure(cx,operation);
  SetTeeFlags(operation,TeeFlags(operation)|TReading);
  if (!request) {
    if (ReadDefaultReaderInto(*state,reader,target)) return true;
    return (TeeFlags(operation)&TReading)?TeeOperationFailure(cx,operation):false;
  }
  JS::RootedObject source(cx,SlotObject(operation,TSource)),buffer(cx);
  JS::RootedValue view(cx,JS::GetReservedSlot(request,YView));
  ByteView info;
  if (!ConvertByteView(cx,view,&info,&buffer,"ReadableStreamBYOBReader.read"))
    return TeeOperationFailure(cx,operation);
  DisturbReadable(source);
  if (ReadableStatus(source)==StreamStatus::Errored) {
    JS::RootedValue reason(cx,JS::GetReservedSlot(source,SError));
    return RejectReadCompletion(cx,target,reason) || TeeOperationFailure(cx,operation);
  }
  if (!ByteRead(cx,source,target,buffer,info,1))
    return (TeeFlags(operation)&TReading)?TeeOperationFailure(cx,operation):false;
  return ScheduleEnd(*state,source) || TeeOperationFailure(cx,operation);
}
bool TeeByteAgain(JSContext* cx,JS::HandleObject operation) {
  SetTeeFlags(operation,TeeFlags(operation)&~TReading);
  if (TeeFlags(operation)&TReadAgain1) return TeePull(cx,operation,0);
  if (TeeFlags(operation)&TReadAgain2) return TeePull(cx,operation,1);
  return true;
}
}
