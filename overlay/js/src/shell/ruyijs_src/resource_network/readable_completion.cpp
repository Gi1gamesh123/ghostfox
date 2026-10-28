// Native input EOF arrives as a resource completion task, after chunk reactions.
#include "resource_network/readable_stream.h"
#include "resource_network/readable_byte.h"
#include "resource_network/state.h"
#include "jsfriendapi.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
namespace {
bool EndTask(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject stream(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
  JSAutoRealm realm(cx,stream);
  SetFlags(stream,Flags(stream)&~EndScheduled);
  args.rval().setUndefined();
  // releaseLock before delivery leaves the original bytes for the next reader.
  if (!(Flags(stream)&Delivered) || ReadableStatus(stream)!=StreamStatus::Readable) return true;
  JS::RootedObject controller(cx,SlotObject(stream,SController));
  if (!ByteClose(cx,controller,ByteCloseKind::Buffered)) return false;
  // Native EOF completes empty/partially filled BYOB descriptors itself.
  return ReadableStatus(stream)!=StreamStatus::Closed || !SlotObject(controller,BPendingHead) ||
      ByteRespond(cx,controller,0);
}
}
bool ScheduleEnd(State& owner,JS::HandleObject stream) {
  if (!StreamBody(stream) || ReadableStatus(stream)==StreamStatus::Closed || (Flags(stream)&EndScheduled)) return true;
  JSContext* cx=owner.cx;
  JSAutoRealm realm(cx,stream);
  JS::RootedFunction callback(cx,js::NewFunctionWithReserved(cx,EndTask,0,0,nullptr));
  if (!callback) return false;
  JS::RootedObject task(cx,JS_GetFunctionObject(callback));
  js::SetFunctionNativeReserved(task,0,JS::ObjectValue(*stream));
  if (!owner.keepaliveTask) return Error(cx,"ReadableStream completion task owner is not bound");
  if (!owner.keepaliveTask(owner.keepaliveTaskContext,task)) return false;
  SetFlags(stream,Flags(stream)|EndScheduled);
  return true;
}
}
