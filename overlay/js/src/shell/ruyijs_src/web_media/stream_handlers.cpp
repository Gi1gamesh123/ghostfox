// EventHandler IDL attributes of MediaStream (onaddtrack/onremovetrack) and
// MediaStreamTrack (onmute/onunmute/onended): the handler value lives in one
// slot, a per-object listener function in another, and registration goes
// through the realm's own EventTarget add/removeEventListener.
#include "web_media/stream.h"
#include "web_media/track.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/Object.h"
#include "js/ValueArray.h"
#include "js/Wrapper.h"
namespace ruyijs::web_media {
namespace {
// The listener's reserved slot 1 packs the handler slot and the object's
// owner-state slot: handler | owner << 8.
bool InvokeHandler(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx, &js::GetFunctionNativeReserved(&args.callee(), 0).toObject());
  const int packed = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  JS::RootedValue callback(cx, JS::GetReservedSlot(self, packed & 0xff)), result(cx);
  args.rval().setUndefined();
  if (!callback.isObject() || !JS::IsCallable(&callback.toObject())) return true;
  if (!JS::Call(cx, self, callback, JS::HandleValueArray(args), &result)) return false;
  if (result.isBoolean() && !result.toBoolean()) {
    auto& state = *static_cast<State*>(JS::GetReservedSlot(self, packed >> 8).toPrivate());
    JS::RootedValue cancel(cx, state.streamPreventDefault);
    if (!JS::Call(cx, args.get(0), cancel, JS::HandleValueArray::empty(), &result)) return false;
  }
  return true;
}
bool Listen(State& state, JS::HandleObject self, JS::HandleValue listener, const char* type, bool add) {
  JSContext* cx = state.cx;
  JS::RootedString name(cx, JS_NewStringCopyZ(cx, type));
  JS::RootedValueArray<2> values(cx);
  JS::RootedValue method(cx, add ? state.addListener : state.removeListener), ignored(cx);
  if (!name) return false;
  values[0].setString(name); values[1].set(listener);
  return JS::Call(cx, self, method, values, &ignored);
}
}
bool SetEventHandler(State& state, JS::HandleObject self, const EventHandlerSlots& slots,
                     JS::HandleValue input) {
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, self);
  JS::RootedValue next(cx, input), previous(cx, JS::GetReservedSlot(self, slots.handler));
  if (!JS_WrapValue(cx, &next)) return false;
  if (!next.isObject()) next.setNull();
  JS::RootedValue listener(cx, JS::GetReservedSlot(self, slots.listener));
  if (next.isObject() && !listener.isObject()) {
    JS::RootedFunction function(cx, js::NewFunctionWithReserved(cx, InvokeHandler, 1, 0, nullptr));
    if (!function) return false;
    JS::RootedObject object(cx, JS_GetFunctionObject(function));
    js::SetFunctionNativeReserved(object, 0, JS::ObjectValue(*self));
    js::SetFunctionNativeReserved(object, 1, JS::Int32Value(slots.handler | slots.owner << 8));
    listener.setObject(*object);
    JS::SetReservedSlot(self, slots.listener, listener);
  }
  if (previous.isObject() && next.isNull() && !Listen(state, self, listener, slots.type, false)) return false;
  if (!previous.isObject() && next.isObject() && !Listen(state, self, listener, slots.type, true)) return false;
  JS::SetReservedSlot(self, slots.handler, next);
  return true;
}
bool StreamSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!StreamReceiver(cx, args, &self)) return false;
  auto& state = *static_cast<State*>(JS::GetReservedSlot(self, StreamOwner).toPrivate());
  const int slot = WebKit::Slot(args);
  const bool add = slot == StreamAddHandler;
  const EventHandlerSlots slots{slot, add ? StreamAddListener : StreamRemoveListener, StreamOwner,
                                add ? "addtrack" : "removetrack"};
  if (!SetEventHandler(state, self, slots, args.get(0))) return false;
  args.rval().setUndefined(); return true;
}
}
