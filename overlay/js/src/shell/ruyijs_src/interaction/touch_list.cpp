#include "interaction/touch.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/ForOfIterator.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::interaction {
bool IsTouchList(JSObject* object) {
  return object && JS::GetClass(object) == &kTouchListClass;
}
JSObject* TouchListReceiver(JSContext* cx, JS::HandleValue value) {
  JSObject* object = value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr;
  if (IsTouchList(object)) return object;
  Error(cx, "TypeError", "TouchList receiver is incompatible");
  return nullptr;
}
bool TouchListLength(JSContext* cx, JS::HandleObject list, uint32_t* length) {
  JSAutoRealm enter(cx, list);
  JS::RootedObject array(cx, &js::GetProxyReservedSlot(list, 0).toObject());
  return JS::GetArrayLength(cx, array, length);
}
bool TouchListItem(JSContext* cx, JS::HandleObject list, uint32_t index,
                   JS::MutableHandleValue result) {
  {
    JSAutoRealm enter(cx, list);
    JS::RootedObject array(cx, &js::GetProxyReservedSlot(list, 0).toObject());
    uint32_t length;
    if (!JS::GetArrayLength(cx, array, &length)) return false;
    if (index >= length) result.setNull();
    else if (!JS_GetElement(cx, array, index, result)) return false;
  }
  return JS_WrapValue(cx, result);
}
bool AppendTouch(JSContext* cx, JS::HandleObject list, JS::HandleValue touch) {
  JSAutoRealm enter(cx, list);
  JS::RootedObject array(cx, &js::GetProxyReservedSlot(list, 0).toObject());
  JS::RootedValue member(cx, touch);
  uint32_t length;
  if (!JS::GetArrayLength(cx, array, &length) || !JS_WrapValue(cx, &member)) return false;
  if (length >= 100000) return Error(cx, "RangeError", "TouchList member limit exceeded");
  return JS_DefineElement(cx, array, length, member, JSPROP_ENUMERATE);
}
bool NewTouchList(State& state, JS::MutableHandleObject result) {
  JSContext* cx = state.cx;
  {
    JSAutoRealm enter(cx, state.page);
    JS::RootedObject array(cx, JS::NewArrayObject(cx, 0));
    JS::RootedObject expando(cx, JS_NewObjectWithGivenProto(cx, nullptr, nullptr));
    if (!array || !expando || !state.touchListPrototype) return false;
    JS::RootedValue private_value(cx, JS::ObjectValue(*expando));
    js::ProxyOptions options; options.setClass(&kTouchListClass);
    result.set(js::NewProxyObject(cx, TouchListHandler(), private_value,
                                 state.touchListPrototype, options));
    if (!result) return false;
    js::SetProxyReservedSlot(result, 0, JS::ObjectValue(*array));
  }
  return JS_WrapObject(cx, result);
}
bool ConvertTouchSequence(State& state, JS::HandleValue input,
                           JS::MutableHandleObject result) {
  JSContext* cx = state.cx;
  if (!NewTouchList(state, result)) return false;
  if (input.isUndefined()) return true;
  if (!input.isObject()) return Error(cx, "TypeError", "Touch list requires a sequence");
  JS::ForOfIterator iterator(cx);
  if (!iterator.init(input)) return false;
  JS::RootedValue member(cx);
  while (true) {
    bool done;
    if (!iterator.next(&member, &done)) return false;
    if (done) return true;
    if (!TouchReceiver(cx, member) || !AppendTouch(cx, result, member)) return false;
  }
}
namespace {
bool Length(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx, TouchListReceiver(cx, args.thisv()));
  uint32_t length;
  if (!self || !TouchListLength(cx, self, &length)) return false;
  args.rval().setNumber(length); return true;
}
bool Item(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx, TouchListReceiver(cx, args.thisv()));
  if (!self) return false;
  if (!argc) return Error(cx, "TypeError", "TouchList.item requires an index");
  uint32_t index;
  return JS::ToUint32(cx, args[0], &index) && TouchListItem(cx, self, index, args.rval());
}
}
extern const JSPropertySpec kTouchListProperties[] = {
  JS_PSG("length", Length, JSPROP_ENUMERATE), JS_PS_END};
extern const JSFunctionSpec kTouchListMethods[] = {
  JS_FN("item", Item, 1, JSPROP_ENUMERATE), JS_FS_END};
}
