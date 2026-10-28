#include "web_media/media_runtime.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/CallAndConstruct.h"
#include "js/PropertyAndElement.h"
#include "js/ValueArray.h"
#include "js/Wrapper.h"
#include "js/Exception.h"
#include "js/Promise.h"
namespace ruyijs::web_media {
namespace { thread_local State* activeState = nullptr; }
void RegisterState(State& state) {
  state.previous = activeState;
  activeState = &state;
}
void UnregisterState(State& state) {
  State** link = &activeState;
  while (*link && *link != &state) link = &(*link)->previous;
  if (*link) *link = state.previous;
}
State* StateForGlobal(JSObject* global) {
  for (State* state = activeState; state; state = state->previous)
    if (state->kit.page == global) return state;
  return nullptr;
}
bool MediaReceiver(JSContext* cx, JS::HandleValue value, const JSClass* clasp,
                   JS::MutableHandleObject object) {
  object.set(value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  return (object && JS::GetClass(object) == clasp) || WebKit::Fail(cx, "Illegal media receiver");
}
bool MediaGetSlot(JSContext* cx, JS::HandleObject object, unsigned slot, JS::MutableHandleValue out) {
  out.set(JS::GetReservedSlot(object, slot));
  return JS_WrapValue(cx, out);
}
bool MediaHandlerSet(State& state, JS::HandleObject self, unsigned slot,
                     const char* type, JS::HandleValue input) {
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, self);
  JS::RootedValue next(cx, input), previous(cx, JS::GetReservedSlot(self, slot));
  JS::RootedValue add(cx, state.addListener), remove(cx, state.removeListener), ignored(cx);
  JS::RootedString name(cx, JS_NewStringCopyZ(cx, type));
  if (!name || !JS_WrapValue(cx, &next) || !JS_WrapValue(cx, &add) || !JS_WrapValue(cx, &remove)) return false;
  JS::RootedValueArray<2> values(cx);
  values[0].setString(name); values[1].set(previous);
  if (previous.isObject() && !JS::Call(cx, self, remove, values, &ignored)) return false;
  if (!next.isObject() || !JS::IsCallable(&next.toObject())) next.setNull();
  values[1].set(next);
  if (next.isObject() && !JS::Call(cx, self, add, values, &ignored)) return false;
  JS::SetReservedSlot(self, slot, next); return true;
}
bool MediaException(State& state, const char* name, const char* message) {
  JSContext* cx = state.cx;
  JS::RootedValue reason(cx);
  {
    JSAutoRealm realm(cx, state.kit.page);
    JS::RootedString label(cx, JS_NewStringCopyZ(cx, name)), text(cx, JS_NewStringCopyZ(cx, message));
    JS::RootedValueArray<2> values(cx);
    JS::RootedObject error(cx);
    if (!label || !text) return false;
    values[0].setString(text); values[1].setString(label);
    if (!JS::Construct(cx, state.exceptionConstructor, values, &error)) return false;
    reason.setObject(*error);
  }
  if (!JS_WrapValue(cx, &reason)) return false;
  JS_SetPendingException(cx, reason); return false;
}
bool MediaInvalidState(State& state) {
  return MediaException(state, "InvalidStateError", "An attempt was made to use an object that is not, or is no longer, usable");
}
bool RejectMediaException(JSContext* cx, JS::HandleObject target, JS::MutableHandleValue out) {
  JS::RootedValue reason(cx);
  if (JS_IsThrowingOutOfMemory(cx) || !JS_GetPendingException(cx, &reason)) return false;
  JS_ClearPendingException(cx);
  JS::RootedObject promise(cx);
  {
    JSAutoRealm realm(cx, target);
    if (!JS_WrapValue(cx, &reason)) return false;
    promise = JS::NewPromiseObject(cx, nullptr);
    if (!promise || !JS::RejectPromise(cx, promise, reason)) return false;
  }
  if (!JS_WrapObject(cx, &promise)) return false;
  out.setObject(*promise); return true;
}
bool MediaConstructorLast(State& state, WebFace& face) {
  return JS_DeleteProperty(state.cx, face.prototype, "constructor") &&
      JS_DefineProperty(state.cx, face.prototype, "constructor", face.constructor, 0);
}
}
