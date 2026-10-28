// Finalized numeric records never contain a JS edge; every edge is a traced slot.
#include "resource_network/readable_byte.h"
#include "resource_network/state.h"
#include "js/Realm.h"
#include <limits>
#include <new>

namespace ruyijs::resource_network {
namespace {
template<class T,unsigned Slot> void Finalize(JS::GCContext*,JSObject* object) {
  auto value=JS::GetReservedSlot(object,Slot);
  if (!value.isUndefined()) delete static_cast<T*>(value.toPrivate());
}
const JSClassOps kByteOps={nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,Finalize<ByteState,BMeta>};
const JSClassOps kQueueOps={nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,Finalize<ByteRegion,BQMeta>};
const JSClassOps kPullOps={nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,Finalize<PullInto,PMeta>};
}
const JSClass kByteControllerClass={"ReadableByteStreamController",JSCLASS_HAS_RESERVED_SLOTS(BSlots)|JSCLASS_FOREGROUND_FINALIZE,&kByteOps};
const JSClass kByteQueueClass={"Resource byte region",JSCLASS_HAS_RESERVED_SLOTS(BQSlots)|JSCLASS_FOREGROUND_FINALIZE,&kQueueOps};
const JSClass kPullIntoClass={"Resource pull into",JSCLASS_HAS_RESERVED_SLOTS(PSlots)|JSCLASS_FOREGROUND_FINALIZE,&kPullOps};
ByteState& ByteData(JSObject* object) { return *static_cast<ByteState*>(JS::GetReservedSlot(object,BMeta).toPrivate()); }
ByteRegion& RegionData(JSObject* object) { return *static_cast<ByteRegion*>(JS::GetReservedSlot(object,BQMeta).toPrivate()); }
PullInto& PullData(JSObject* object) { return *static_cast<PullInto*>(JS::GetReservedSlot(object,PMeta).toPrivate()); }
bool IsByteStream(JSObject* stream) {
  JSObject* controller=SlotObject(stream,SController);
  return controller && JS::GetClass(controller)==&kByteControllerClass;
}
bool NewByteController(JSContext* cx,JS::HandleObject stream,JS::HandleObject prototype,
                       JS::HandleObject requestPrototype,uint64_t allocation,double hwm,JS::MutableHandleObject result) {
  result.set(JS_NewObjectWithGivenProto(cx,&kByteControllerClass,prototype));
  if (!result) return false;
  auto* data=new(std::nothrow) ByteState{0,allocation};
  if (!data) {JS_ReportOutOfMemory(cx);return false;}
  JS::SetReservedSlot(result,BMeta,JS::PrivateValue(data));
  JS::SetReservedSlot(result,DStream,JS::ObjectValue(*stream));
  JS::SetReservedSlot(result,DHwm,JS::DoubleValue(hwm));
  JS::SetReservedSlot(result,BRequestPrototype,JS::ObjectValue(*requestPrototype));
  SetDefaultFlags(result,0);
  JS::SetReservedSlot(stream,SController,JS::ObjectValue(*result));
  return true;
}
double ByteDesiredSize(JSObject* controller) {
  return JS::GetReservedSlot(controller,DHwm).toNumber()-double(ByteData(controller).total);
}
void ResetByteQueue(JSObject* controller) {
  JS::SetReservedSlot(controller,DHead,JS::UndefinedValue());
  JS::SetReservedSlot(controller,DTail,JS::UndefinedValue());
  ByteData(controller).total=0;
}
bool AppendByteRegion(JSContext* cx,JS::HandleObject controller,JS::HandleObject buffer,uint64_t offset,uint64_t length) {
  if (!length) return true;
  if (length>std::numeric_limits<uint64_t>::max()-ByteData(controller).total)
    return Error(cx,"Byte queue size overflow",true);
  JSAutoRealm realm(cx,controller);
  JS::RootedObject entry(cx,JS_NewObjectWithGivenProto(cx,&kByteQueueClass,nullptr));
  if (!entry || !StoreByteBuffer(cx,entry,BQBuffer,buffer)) return false;
  auto* region=new(std::nothrow) ByteRegion{offset,length};
  if (!region) {JS_ReportOutOfMemory(cx);return false;}
  JS::SetReservedSlot(entry,BQMeta,JS::PrivateValue(region));
  if (JSObject* tail=SlotObject(controller,DTail)) JS::SetReservedSlot(tail,BQNext,JS::ObjectValue(*entry));
  else JS::SetReservedSlot(controller,DHead,JS::ObjectValue(*entry));
  JS::SetReservedSlot(controller,DTail,JS::ObjectValue(*entry));
  ByteData(controller).total+=length;return true;
}
bool NewPullInto(JSContext* cx,JS::HandleObject controller,JS::HandleObject buffer,
                  const PullInto& data,JS::MutableHandleObject result) {
  JSAutoRealm realm(cx,controller);
  result.set(JS_NewObjectWithGivenProto(cx,&kPullIntoClass,nullptr));
  if (!result || !StoreByteBuffer(cx,result,PBuffer,buffer)) return false;
  auto* record=new(std::nothrow) PullInto(data);
  if (!record) {JS_ReportOutOfMemory(cx);return false;}
  JS::SetReservedSlot(result,PMeta,JS::PrivateValue(record));return true;
}
void AppendPullInto(JSObject* controller,JSObject* descriptor) {
  if (JSObject* tail=SlotObject(controller,BPendingTail)) JS::SetReservedSlot(tail,PNext,JS::ObjectValue(*descriptor));
  else JS::SetReservedSlot(controller,BPendingHead,JS::ObjectValue(*descriptor));
  JS::SetReservedSlot(controller,BPendingTail,JS::ObjectValue(*descriptor));
}
JSObject* ShiftPullInto(JSObject* controller) {
  JSObject* first=SlotObject(controller,BPendingHead);
  if (!first) return nullptr;
  auto next=JS::GetReservedSlot(first,PNext);
  JS::SetReservedSlot(controller,BPendingHead,next);
  if (!next.isObject()) JS::SetReservedSlot(controller,BPendingTail,JS::UndefinedValue());
  JS::SetReservedSlot(first,PNext,JS::UndefinedValue());return first;
}
}
