// Reader.closed observes source errors; obsolete byte readers cannot affect branches.
#include "resource_network/readable_tee.h"
#include "resource_network/readable_controller.h"
#include "resource_network/readable_byte.h"
#include "resource_network/state.h"
#include "jsfriendapi.h"
#include "js/Promise.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
bool TeeFinishCancel(JSContext* cx,JS::HandleObject operation) {
  if (TeeCanceled(operation,0) && TeeCanceled(operation,1)) return true;
  JS::RootedObject promise(cx,SlotObject(operation,TCancelPromise));
  return JS::ResolvePromise(cx,promise,JS::UndefinedHandleValue);
}
bool TeeErrorBranches(JSContext* cx,JS::HandleObject operation,JS::HandleValue reason) {
  for (unsigned i=0;i<2;i++) {
    JS::RootedObject branch(cx,SlotObject(operation,i?TBranch2:TBranch1));
    JS::RootedObject controller(cx,SlotObject(branch,SController));
    if (!DefaultError(cx,controller,reason)) return false;
  }
  return TeeFinishCancel(cx,operation);
}
bool TeeCloseBranches(JSContext* cx,JS::HandleObject operation) {
  for (unsigned i=0;i<2;i++) {
    JS::RootedObject branch(cx,SlotObject(operation,i?TBranch2:TBranch1));
    JS::RootedObject controller(cx,SlotObject(branch,SController));
    if (TeeCanceled(operation,i) || ReadableStatus(branch)!=StreamStatus::Readable ||
        (DefaultFlags(controller)&DCloseRequested)) continue;
    if (!DefaultClose(cx,controller)) return false;
  }
  return TeeFinishCancel(cx,operation);
}
namespace {
bool ReaderRejected(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject operation(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
  JS::RootedObject reader(cx,&js::GetFunctionNativeReserved(&args.callee(),1).toObject());
  args.rval().setUndefined();
  if (reader!=SlotObject(operation,TReader)) return true;
  JSAutoRealm realm(cx,operation);
  JS::RootedValue reason(cx,args.get(0));
  return (JS_WrapValue(cx,&reason) && TeeErrorBranches(cx,operation,reason)) ||
      TeeOperationFailure(cx,operation);
}
}
bool TeeObserveReader(JSContext* cx,JS::HandleObject operation,JS::HandleObject reader) {
  JS::RootedObject closed(cx,SlotObject(reader,RClosed)),fulfilled(cx);
  JS::RootedFunction callback(cx,js::NewFunctionWithReserved(cx,ReaderRejected,1,0,nullptr));
  if (!callback) return false;
  JS::RootedObject rejected(cx,JS_GetFunctionObject(callback));
  js::SetFunctionNativeReserved(rejected,0,JS::ObjectValue(*operation));
  js::SetFunctionNativeReserved(rejected,1,JS::ObjectValue(*reader));
  return JS::AddPromiseReactions(cx,closed,fulfilled,rejected);
}
}
