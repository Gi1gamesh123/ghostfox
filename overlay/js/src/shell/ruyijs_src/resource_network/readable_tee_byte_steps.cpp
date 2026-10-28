// Static completion tables bind BYOB branch identity without a public selector.
#include "resource_network/readable_tee_byte.h"
#include "resource_network/readable_byte.h"
#include "jsfriendapi.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
namespace {
template<int Branch> bool Task(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject operation(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
  JSAutoRealm realm(cx,operation);
  JS::RootedValue chunk(cx,js::GetFunctionNativeReserved(&args.callee(),1));
  args.rval().setUndefined();
  return TeeCompleteMicrotask(cx,TeeByteChunk(cx,operation,chunk,Branch));
}
template<int Branch> bool Chunk(JSContext* cx,JS::HandleObject,JS::HandleObject operation,JS::HandleValue chunk) {
  return TeeQueueChunk(cx,operation,chunk,Task<Branch>) || TeeOperationFailure(cx,operation);
}
template<int Branch> bool Close(JSContext* cx,JS::HandleObject,JS::HandleObject operation,JS::HandleValue chunk) {
  SetTeeFlags(operation,TeeFlags(operation)&~TReading);
  return TeeByteClose(cx,operation,chunk,Branch);
}
bool Fail(JSContext*,JS::HandleObject,JS::HandleObject operation,JS::HandleValue) {
  SetTeeFlags(operation,TeeFlags(operation)&~TReading);return true;
}
}
const ReadCompletionSteps kByteDefaultTeeSteps={Chunk<-1>,Close<-1>,Fail};
const ReadCompletionSteps kByteByob1TeeSteps={Chunk<0>,Close<0>,Fail};
const ReadCompletionSteps kByteByob2TeeSteps={Chunk<1>,Close<1>,Fail};
}
