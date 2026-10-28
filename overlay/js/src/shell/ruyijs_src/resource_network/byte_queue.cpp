// Exact byte regions and descriptor fill; only storage enters controller realm.
#include "resource_network/readable_byte.h"
#include "resource_network/state.h"
#include "js/ArrayBuffer.h"
#include <algorithm>

namespace ruyijs::resource_network {
bool CloneByteRegion(JSContext* cx,JS::HandleObject controller,JS::HandleObject buffer,uint64_t at,uint64_t size) {
  if (!size) return true;
  JS::RootedObject clone(cx,JS::ArrayBufferClone(cx,buffer,size_t(at),size_t(size)));
  if (!clone) return ByteFailure(cx,controller);
  return AppendByteRegion(cx,controller,clone,0,size) || ByteCommitFailure(cx,controller);
}
bool DetachPullInto(JSContext* cx,JS::HandleObject controller) {
  JS::RootedObject first(cx,SlotObject(controller,BPendingHead));
  if (!first) return true;
  const auto& data=PullData(first);
  JS::RootedObject buffer(cx,SlotObject(first,PBuffer));
  if (!CloneByteRegion(cx,controller,buffer,data.offset,data.filled)) return false;
  ShiftPullInto(controller);return true;
}
bool FillPullInto(JSContext* cx,JS::HandleObject controller,JS::HandleObject descriptor,bool* ready) {
  auto& data=PullData(descriptor);
  uint64_t count=std::min(ByteData(controller).total,data.capacity-data.filled);
  const uint64_t maximum=data.filled+count,aligned=maximum-maximum%data.element;
  *ready=aligned>=data.minimum;
  if (*ready) count=aligned-data.filled;
  JS::RootedObject destination(cx,SlotObject(descriptor,PBuffer));
  while (count) {
    JS::RootedObject entry(cx,SlotObject(controller,DHead)),source(cx,SlotObject(entry,BQBuffer));
    auto& region=RegionData(entry);
    const uint64_t copied=std::min(count,region.length);
    if (!CopyByteRegion(cx,destination,data.offset+data.filled,source,region.offset,copied)) return false;
    region.offset+=copied;region.length-=copied;
    if (!region.length) {
      auto next=JS::GetReservedSlot(entry,BQNext);JS::SetReservedSlot(controller,DHead,next);
      if (!next.isObject()) JS::SetReservedSlot(controller,DTail,JS::UndefinedValue());
    }
    ByteData(controller).total-=copied;data.filled+=copied;count-=copied;
  }
  return true;
}
// Respond processes descriptors even after a reader is replaced. It must
// leave ordinary default-reader requests to the enqueue/read algorithms.
bool ProcessPullIntoQueue(JSContext* cx,JS::HandleObject controller) {
  while (ByteData(controller).total && SlotObject(controller,BPendingHead)) {
    JS::RootedObject descriptor(cx,SlotObject(controller,BPendingHead));
    bool ready=false;
    if (!FillPullInto(cx,controller,descriptor,&ready)) return false;
    if (!ready) break;
    ShiftPullInto(controller);
    if (!CommitPullInto(cx,controller,descriptor,false)) return false;
  }
  return true;
}
bool ProcessByteQueue(JSContext* cx,JS::HandleObject controller) {
  JS::RootedObject stream(cx,SlotObject(controller,DStream));
  JSObject* reader=SlotObject(stream,SReader);
  if (!reader) return true;
  if (!IsByobReader(reader)) {
    while (ByteData(controller).total && SlotObject(stream,SHead)) {
      JS::RootedObject entry(cx,SlotObject(controller,DHead)),buffer(cx,SlotObject(entry,BQBuffer));
      const auto data=RegionData(entry);
      JS::RootedObject request(cx,SlotObject(stream,SHead)),promise(cx,SlotObject(request,QTarget)),view(cx);
      if (!NewByteView(cx,JS::Scalar::Uint8,buffer,data.offset,data.length,&view)) return ByteCommitFailure(cx,controller);
      JS::RootedValue value(cx,JS::ObjectValue(*view));
      const bool native=IsNativeReadCompletion(promise);
      if (!native && !CompleteRead(cx,promise,false,value)) return ByteCommitFailure(cx,controller);
      auto next=JS::GetReservedSlot(entry,BQNext);JS::SetReservedSlot(controller,DHead,next);
      if (!next.isObject()) JS::SetReservedSlot(controller,DTail,JS::UndefinedValue());
      ByteData(controller).total-=data.length;
      JS::RootedObject descriptor(cx,SlotObject(controller,BPendingHead));
      if (descriptor && PullData(descriptor).kind==ReadKind::Default) {InvalidateByobRequest(controller);ShiftPullInto(controller);}
      PopReadRequest(stream);
      if (native && !CompleteRead(cx,promise,false,value)) {
        FailNativeReadCompletion(cx,promise);return ByteCommitFailure(cx,controller);
      }
    }
    return true;
  }
  return ProcessPullIntoQueue(cx,controller);
}
bool ByteQueueDrained(JSContext* cx,JS::HandleObject controller) {
  JS::RootedObject stream(cx,SlotObject(controller,DStream));
  if (!ByteData(controller).total && (DefaultFlags(controller)&DCloseRequested)) {
    ClearDefaultAlgorithms(controller);return CloseReadable(cx,stream);
  }
  return DefaultPullIfNeeded(cx,controller);
}
}
