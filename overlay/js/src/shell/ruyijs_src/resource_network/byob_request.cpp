// Cached request identity belongs to the controller; its view uses getter realm.
#include "resource_network/readable_byte.h"
#include "resource_network/state.h"
#include "js/ArrayBuffer.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
const JSClass kByobRequestClass={"ReadableStreamBYOBRequest",JSCLASS_HAS_RESERVED_SLOTS(YSlots)};
JSObject* ByobRequestReceiver(JS::HandleValue value) {
  JSObject* object=value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr;
  return object && JS::GetClass(object)==&kByobRequestClass?object:nullptr;
}
void InvalidateByobRequest(JSObject* controller) {
  if (JSObject* request=SlotObject(controller,BRequest)) {
    JS::SetReservedSlot(request,YController,JS::UndefinedValue());
    JS::SetReservedSlot(request,YView,JS::NullValue());
    JS::SetReservedSlot(controller,BRequest,JS::UndefinedValue());
  }
}
bool GetByobRequest(JSContext* cx,JS::HandleObject controller,JS::MutableHandleObject result) {
  result.set(SlotObject(controller,BRequest));
  if (result) return JS_WrapObject(cx,result);
  JS::RootedObject first(cx,SlotObject(controller,BPendingHead));
  if (!first) return true;
  const auto& data=PullData(first);
  JS::RootedObject buffer(cx,SlotObject(first,PBuffer)),view(cx);
  if (!NewByteView(cx,JS::Scalar::Uint8,buffer,data.offset+data.filled,data.capacity-data.filled,&view)) return false;
  {
    JSAutoRealm realm(cx,controller);
    JS::RootedObject prototype(cx,SlotObject(controller,BRequestPrototype));
    result.set(JS_NewObjectWithGivenProto(cx,&kByobRequestClass,prototype));
    if (!result || !StoreByteBuffer(cx,result,YView,view)) return false;
    JS::SetReservedSlot(result,YController,JS::ObjectValue(*controller));
    JS::SetReservedSlot(controller,BRequest,JS::ObjectValue(*result));
  }
  return JS_WrapObject(cx,result);
}
namespace {
bool Construct(JSContext* cx,unsigned,JS::Value*) { return Error(cx,"Illegal constructor."); }
bool View(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject request(cx,ByobRequestReceiver(args.thisv()));
  if (!request) return Error(cx,"'get view' called on an object that does not implement interface ReadableStreamBYOBRequest.");
  args.rval().set(JS::GetReservedSlot(request,YView));return JS_WrapValue(cx,args.rval());
}
bool Respond(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject request(cx,ByobRequestReceiver(args.thisv()));
  if (!request) return Error(cx,"'respond' called on an object that does not implement interface ReadableStreamBYOBRequest.");
  if (!argc) return Error(cx,"ReadableStreamBYOBRequest.respond: At least 1 argument required, but only 0 passed");
  uint64_t written;
  if (!EnforceU64(cx,args.get(0),&written,"ReadableStreamBYOBRequest.respond: Argument 1")) return false;
  JS::RootedObject controller(cx,SlotObject(request,YController));
  if (!controller) return Error(cx,"ReadableStreamBYOBRequest.respond: Undefined Controller");
  JS::RootedObject first(cx,SlotObject(controller,BPendingHead)),buffer(cx,SlotObject(first,PBuffer));
  if (JS::IsDetachedArrayBufferObject(buffer)) return Error(cx,"ReadableStreamBYOBRequest.respond: View of Detached buffer");
  if (!ByteRespond(cx,controller,written)) return false;
  args.rval().setUndefined();return true;
}
bool RespondNew(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject request(cx,ByobRequestReceiver(args.thisv()));
  if (!request) return Error(cx,"'respondWithNewView' called on an object that does not implement interface ReadableStreamBYOBRequest.");
  if (!argc) return Error(cx,"ReadableStreamBYOBRequest.respondWithNewView: At least 1 argument required, but only 0 passed");
  ByteView info;JS::RootedObject buffer(cx);
  if (!ConvertByteView(cx,args.get(0),&info,&buffer,"ReadableStreamBYOBRequest.respondWithNewView")) return false;
  JS::RootedObject controller(cx,SlotObject(request,YController));
  if (!controller) return Error(cx,"ReadableStreamBYOBRequest.respondWithNewView: Undefined Controller");
  if (!ByteRespondNewView(cx,controller,args.get(0))) return false;
  args.rval().setUndefined();return true;
}
}
bool InstallByobRequest(State& state) {
  JS::RootedObject parent(state.cx,JS::GetRealmObjectPrototype(state.cx));
  auto& kit=state.kit;auto& face=state.byobRequest;
  return parent && kit.Define("ReadableStreamBYOBRequest",&kByobRequestClass,parent,Construct,&face) &&
      kit.Method(face,"ReadableStreamBYOBRequest","respond",Respond,1,0) &&
      kit.Method(face,"ReadableStreamBYOBRequest","respondWithNewView",RespondNew,1,0) &&
      kit.Accessor(face,"ReadableStreamBYOBRequest","view",View,nullptr,0);
}
}
