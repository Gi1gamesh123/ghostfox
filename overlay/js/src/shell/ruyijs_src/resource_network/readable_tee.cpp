// tee owns coordination only; original stream/controller factories own branches.
#include "resource_network/readable_tee.h"
#include "resource_network/readable_controller.h"
#include "resource_network/readable_byte.h"
#include "resource_network/state.h"
#include "js/Array.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
const JSClass kTeeOperationClass={"Resource tee operation",JSCLASS_HAS_RESERVED_SLOTS(TSlots)};
namespace {
bool NewBranch(JSContext* cx,JS::HandleObject operation,JS::HandleObject factories,
    bool bytes,JS::MutableHandleObject result) {
  State* state=StreamFactoryState(cx,factories);
  if (!state) return false;
  JS::RootedObject prototype(cx,SlotObject(factories,SFStream));
  JS::RootedObject readerPrototype(cx,SlotObject(factories,SFDefaultReader)),holder(cx),algorithms(cx);
  if (!NewReadableStream(cx,prototype,readerPrototype,holder,result) ||
      !NewNativeSourceAlgorithms(cx,operation,kTeeSourceSteps,&algorithms)) return false;
  JS::SetReservedSlot(result,SByobPrototype,JS::GetReservedSlot(factories,SFByobReader));
  JS::RootedValue source(cx,JS::ObjectValue(*algorithms));
  return InitializeReadableController(*state,result,source,JS::UndefinedHandleValue,
      JS::UndefinedHandleValue,JS::UndefinedHandleValue,JS::UndefinedHandleValue,bytes?0:1,bytes,0);
}
}
bool StreamTee(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject source(cx,StreamReceiver(args.thisv()));
  if (!source) return Error(cx,"'tee' called on an object that does not implement interface ReadableStream.");
  if (SlotObject(source,SReader)) return Error(cx,
      "ReadableStream.tee: Cannot get a new reader for a readable stream already locked by another reader.");
  JS::RootedObject first(cx),second(cx),operation(cx);
  if (!TeeReadable(cx,source,&first,&second,&operation)) return false;
  JS::RootedObject result(cx,JS::NewArrayObject(cx,2));
  JS::RootedValue left(cx,JS::ObjectValue(*first)),right(cx,JS::ObjectValue(*second));
  if (!result || !JS_WrapValue(cx,&left) || !JS_WrapValue(cx,&right) ||
      !JS_SetElement(cx,result,0,left) || !JS_SetElement(cx,result,1,right))
    return TeeOperationFailure(cx,operation);
  args.rval().setObject(*result);return true;
}
bool TeeReadable(JSContext* cx,JS::HandleObject source,JS::MutableHandleObject first,
                   JS::MutableHandleObject second,JS::MutableHandleObject operation) {
  {
    JSAutoRealm realm(cx,source);
    JS::RootedObject factories(cx),reader(cx),cancel(cx);
    if (!StreamFactories(cx,source,&factories)) return false;
    JS::RootedObject readerPrototype(cx,SlotObject(factories,SFDefaultReader));
    if (!NewDefaultReader(cx,source,readerPrototype,&reader)) return false;
    operation.set(JS_NewObjectWithGivenProto(cx,&kTeeOperationClass,nullptr));
    cancel=JS::NewPromiseObject(cx,nullptr);
    if (!operation || !cancel) return false;
    JS::SetReservedSlot(operation,TSource,JS::ObjectValue(*source));
    JS::SetReservedSlot(operation,TReader,JS::ObjectValue(*reader));
    JS::SetReservedSlot(operation,TCancelPromise,JS::ObjectValue(*cancel));
    JS::SetReservedSlot(operation,TFactories,JS::ObjectValue(*factories));
    SetTeeFlags(operation,0);
    if (!NewBranch(cx,operation,factories,IsByteStream(source),first)) return false;
    JS::SetReservedSlot(operation,TBranch1,JS::ObjectValue(*first));
    if (!NewBranch(cx,operation,factories,IsByteStream(source),second))
      return TeeOperationFailure(cx,operation);
    JS::SetReservedSlot(operation,TBranch2,JS::ObjectValue(*second));
    if (!TeeObserveReader(cx,operation,reader)) return TeeOperationFailure(cx,operation);
  }
  return true;
}
bool TeePull(JSContext* cx,JS::HandleObject operation,unsigned branch) {
  JS::RootedObject source(cx,SlotObject(operation,TSource));
  return IsByteStream(source)?TeePullByte(cx,operation,branch):TeePullDefault(cx,operation);
}
}
