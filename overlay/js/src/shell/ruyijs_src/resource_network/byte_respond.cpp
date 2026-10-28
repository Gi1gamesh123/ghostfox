// Commit response bytes after native transfer, with exact alignment/remainder.
#include "resource_network/readable_byte.h"
#include "resource_network/state.h"
#include "js/ArrayBuffer.h"

namespace ruyijs::resource_network {
bool ByteRespondInternal(JSContext* cx,JS::HandleObject controller,uint64_t written) {
  JS::RootedObject first(cx,SlotObject(controller,BPendingHead)),stream(cx,SlotObject(controller,DStream));
  InvalidateByobRequest(controller);
  if (ReadableStatus(stream)==StreamStatus::Closed) {
    if (PullData(first).kind==ReadKind::None) ShiftPullInto(controller);
    JSObject* reader=SlotObject(stream,SReader);
    if (reader && IsByobReader(reader)) {
      while (SlotObject(stream,SHead)) {
        JS::RootedObject descriptor(cx,ShiftPullInto(controller));
        if (!descriptor || !CommitPullInto(cx,controller,descriptor,true)) return false;
      }
    }
    return true;
  }
  auto& data=PullData(first);data.filled+=written;
  if (data.kind==ReadKind::None) {
    if (!DetachPullInto(cx,controller) || !ProcessPullIntoQueue(cx,controller)) return false;
  } else if (data.filled>=data.minimum) {
    ShiftPullInto(controller);
    const uint64_t remainder=data.filled%data.element;
    if (remainder) {
      JS::RootedObject buffer(cx,SlotObject(first,PBuffer));
      if (!CloneByteRegion(cx,controller,buffer,data.offset+data.filled-remainder,remainder)) return false;
      data.filled-=remainder;
    }
    if (!CommitPullInto(cx,controller,first,false) || !ProcessPullIntoQueue(cx,controller)) return false;
  }
  return DefaultPullIfNeeded(cx,controller);
}
bool ByteRespond(JSContext* cx,JS::HandleObject controller,uint64_t written) {
  JS::RootedObject first(cx,SlotObject(controller,BPendingHead)),stream(cx,SlotObject(controller,DStream));
  if (!first) return Error(cx,"Missing byte pull-into descriptor");
  const auto& data=PullData(first);
  if (ReadableStatus(stream)==StreamStatus::Closed) {
    if (written) return Error(cx,"ReadableStreamBYOBRequest.respond: bytesWritten not zero on closed stream");
  } else {
    if (!written) return Error(cx,"ReadableStreamBYOBRequest.respond: bytesWritten 0");
    if (written>data.capacity-data.filled) return Error(cx,"ReadableStreamBYOBRequest.respond: bytesFilled + bytesWritten > byteLength",true);
  }
  JS::RootedObject old(cx,SlotObject(first,PBuffer)),buffer(cx);
  if (!TransferByteBuffer(cx,old,&buffer))
    return JS::IsDetachedArrayBufferObject(old) ? ByteCommitFailure(cx,controller) : false;
  if (!StoreByteBuffer(cx,first,PBuffer,buffer)) return ByteCommitFailure(cx,controller);
  return ByteRespondInternal(cx,controller,written);
}
bool ByteRespondNewView(JSContext* cx,JS::HandleObject controller,JS::HandleValue value) {
  ByteView info;
  JS::RootedObject input(cx),first(cx,SlotObject(controller,BPendingHead)),stream(cx,SlotObject(controller,DStream));
  if (!ConvertByteView(cx,value,&info,&input,"ReadableStreamBYOBRequest.respondWithNewView")) return false;
  if (!first) return Error(cx,"Missing byte pull-into descriptor");
  if (JS::IsDetachedArrayBufferObject(JS::UnwrapArrayBuffer(input))) return Error(cx,"ReadableStreamBYOBRequest.respondWithNewView: View of Detached Array Buffer");
  const auto& data=PullData(first);
  if (ReadableStatus(stream)==StreamStatus::Closed) {
    if (info.length) return Error(cx,"ReadableStreamBYOBRequest.respondWithNewView: View has non-zero length in closed stream");
  } else if (!info.length) return Error(cx,"ReadableStreamBYOBRequest.respondWithNewView: View has zero length in readable stream");
  if (info.offset!=data.offset+data.filled) return Error(cx,"ReadableStreamBYOBRequest.respondWithNewView: Invalid Offset",true);
  if (info.bufferLength!=data.bufferLength) return Error(cx,"ReadableStreamBYOBRequest.respondWithNewView: Mismatched buffer byte lengths",true);
  if (info.length>data.capacity-data.filled) return Error(cx,"ReadableStreamBYOBRequest.respondWithNewView: Too many bytes",true);
  JS::RootedObject buffer(cx);
  if (!TransferByteBuffer(cx,input,&buffer))
    return JS::IsDetachedArrayBufferObject(JS::UnwrapArrayBuffer(input)) ? ByteCommitFailure(cx,controller) : false;
  if (!StoreByteBuffer(cx,first,PBuffer,buffer)) return ByteCommitFailure(cx,controller);
  return ByteRespondInternal(cx,controller,info.length);
}
}
