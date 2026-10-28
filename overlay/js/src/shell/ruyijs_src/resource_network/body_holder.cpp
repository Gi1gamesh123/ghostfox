// The existing BodyRecord has exactly one GC owner, independent of its wrappers.
#include "resource_network/readable_stream.h"
#include "resource_network/readable_byte.h"
#include "resource_network/state.h"
#include "js/Wrapper.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
namespace {
void FinalizeHolder(JS::GCContext*,JSObject* object) {
  auto value=JS::GetReservedSlot(object,HRecord);
  if (!value.isUndefined()) delete static_cast<BodyRecord*>(value.toPrivate());
}
const JSClassOps kHolderOps={nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,FinalizeHolder};
}
const JSClass kBodyHolderClass={"Resource body holder",JSCLASS_HAS_RESERVED_SLOTS(HSlots)|
    JSCLASS_FOREGROUND_FINALIZE,&kHolderOps};

JSObject* NewBodyHolder(State& state) {
  JS::RootedObject holder(state.cx,JS_NewObjectWithGivenProto(state.cx,&kBodyHolderClass,nullptr)),factories(state.cx);
  if (!holder || !StreamFactories(state.cx,state.page,&factories)) return nullptr;
  JS::SetReservedSlot(holder,HFactories,JS::ObjectValue(*factories));
  JS::SetReservedSlot(holder,HStreamPrototype,JS::ObjectValue(*state.stream.prototype));
  JS::SetReservedSlot(holder,HReaderPrototype,JS::ObjectValue(*state.defaultReader.prototype));
  JS::SetReservedSlot(holder,HBytePrototype,JS::ObjectValue(*state.byteController.prototype));
  JS::SetReservedSlot(holder,HByobPrototype,JS::ObjectValue(*state.byobReader.prototype));
  JS::SetReservedSlot(holder,HRequestPrototype,JS::ObjectValue(*state.byobRequest.prototype));
  // HRecord stays undefined until the caller commits ownership after all allocations.
  return holder;
}
void CommitBodyHolder(JSObject* holder,BodyRecord* record) {
  JS::SetReservedSlot(holder,HRecord,JS::PrivateValue(record));
}
JSObject* BodyReceiver(JS::HandleValue value) {
  JSObject* object=value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr;
  if (!object) return nullptr;
  const JSClass* clasp=JS::GetClass(object);
  if (clasp!=&kResponseClass && clasp!=&kRequestClass) return nullptr;
  return object;
}
JSObject* BodyHolderOf(JS::HandleValue value) {
  JSObject* object=BodyReceiver(value);
  return object?SlotObject(object,0):nullptr;
}
BodyRecord* HolderBody(JSObject* holder) {
  if (!holder || JS::GetClass(holder)!=&kBodyHolderClass) return nullptr;
  auto value=JS::GetReservedSlot(holder,HRecord);
  return value.isUndefined()?nullptr:static_cast<BodyRecord*>(value.toPrivate());
}
BodyRecord* BodyOf(JS::HandleValue value) { return HolderBody(BodyHolderOf(value)); }
BodyRecord* BodyOf(JS::HandleObject object) {
  JSObject* raw=object?js::CheckedUnwrapStatic(object):nullptr;
  if (!raw || (JS::GetClass(raw)!=&kResponseClass && JS::GetClass(raw)!=&kRequestClass)) return nullptr;
  return HolderBody(SlotObject(raw,0));
}
bool BodyLocked(JSObject* holder) {
  JSObject* stream=SlotObject(holder,HStream);
  return stream && !JS::GetReservedSlot(stream,SReader).isUndefined();
}
bool EnsureBodyStream(JSContext* cx,JS::HandleObject holder,JS::MutableHandleObject result) {
  if (JSObject* stream=SlotObject(holder,HStream)) {result.set(stream);return true;}
  JS::RootedObject proto(cx,SlotObject(holder,HStreamPrototype));
  JS::RootedObject readerProto(cx,SlotObject(holder,HReaderPrototype));
  if (!NewReadableStream(cx,proto,readerProto,holder,result)) return false;
  JS::RootedObject bytePrototype(cx,SlotObject(holder,HBytePrototype)),requestPrototype(cx,SlotObject(holder,HRequestPrototype)),controller(cx);
  if (!NewByteController(cx,result,bytePrototype,requestPrototype,0,0,&controller)) return false;
  JS::SetReservedSlot(result,SByobPrototype,JS::GetReservedSlot(holder,HByobPrototype));
  SetDefaultFlags(controller,DStarted);
  if (HolderBody(holder)->used) {
    SetFlags(result,Delivered);
    SetReadableStatus(result,StreamStatus::Closed);
    JS::SetReservedSlot(result,SReader,JS::BooleanValue(true));
  }
  JS::SetReservedSlot(holder,HStream,JS::ObjectValue(*result));
  return true;
}
bool BeginBodyConsume(JSContext* cx,JS::HandleValue receiver) {
  JS::RootedObject holder(cx,BodyHolderOf(receiver));
  BodyRecord* body=HolderBody(holder);
  if (!body || body->nullBody) return true;
  JSAutoRealm realm(cx,holder);
  // A body that has never exposed a stream need not allocate one to be consumed.
  if (JSObject* cached=SlotObject(holder,HStream)) {
    JS::RootedObject stream(cx,cached);
    JS::SetReservedSlot(stream,SReader,JS::BooleanValue(true));
    SetFlags(stream,Flags(stream)|Delivered);
    SetReadableStatus(stream,StreamStatus::Closed);
  }
  body->used=true;
  return true;
}
}
