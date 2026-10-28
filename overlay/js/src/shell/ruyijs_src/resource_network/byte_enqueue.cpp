#include "resource_network/readable_byte.h"
#include "resource_network/state.h"
#include "js/ArrayBuffer.h"

namespace ruyijs::resource_network {
bool ByteEnqueue(JSContext* cx,JS::HandleObject controller,JS::HandleValue chunk) {
  ByteView info;
  JS::RootedObject input(cx),buffer(cx),stream(cx,SlotObject(controller,DStream));
  if (!ConvertByteView(cx,chunk,&info,&input,"ReadableByteStreamController.enqueue")) return false;
  if (!info.length || !info.bufferLength) return Error(cx,"ReadableByteStreamController.enqueue: Zero Length View");
  if (DefaultFlags(controller)&DCloseRequested) return Error(cx,"ReadableByteStreamController.enqueue: Close Requested");
  if (ReadableStatus(stream)!=StreamStatus::Readable) return Error(cx,"ReadableByteStreamController.enqueue: Not Readable");
  if (!TransferByteBuffer(cx,input,&buffer)) return false;
  JS::RootedObject first(cx,SlotObject(controller,BPendingHead));
  if (first) {
    JS::RootedObject old(cx,SlotObject(first,PBuffer)),transferred(cx);
    if (JS::IsDetachedArrayBufferObject(old)) return Error(cx,"Pending PullInto has detached buffer");
    InvalidateByobRequest(controller);
    if (!TransferByteBuffer(cx,old,&transferred))
      return JS::IsDetachedArrayBufferObject(old) ? ByteCommitFailure(cx,controller) : false;
    if (!StoreByteBuffer(cx,first,PBuffer,transferred)) return ByteCommitFailure(cx,controller);
    if (PullData(first).kind==ReadKind::None && !DetachPullInto(cx,controller)) return false;
  }
  if (!AppendByteRegion(cx,controller,buffer,info.offset,info.length)) return ByteCommitFailure(cx,controller);
  return ProcessByteQueue(cx,controller) && DefaultPullIfNeeded(cx,controller);
}
}
