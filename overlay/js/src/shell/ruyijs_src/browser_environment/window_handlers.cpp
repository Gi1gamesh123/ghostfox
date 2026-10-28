#include "browser_environment/window_handlers.h"
#include "interaction/realm.h"
#include "runtime/page_helpers.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/PropertyDescriptor.h"
#include "js/Realm.h"
#include <utility>

namespace ruyijs::browser_environment {
namespace {
const JSClass kHandlerStoreClass = {"WindowHandlers", JSCLASS_HAS_RESERVED_SLOTS(kHandlerStoreSlots)};
unsigned Index(const JS::CallArgs& args) {
  return unsigned(js::GetFunctionNativeReserved(&args.callee(), 1).toInt32());
}
bool Listen(JSContext* cx, JS::HandleObject store, State& owner, unsigned index,
            JS::HandleValue callback, bool adding) {
  JS::RootedValue method(cx, JS::GetReservedSlot(store, adding ? kHandlerAdd : kHandlerRemove));
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, kWindowHandlerNames[index] + 2));
  JS::RootedValueArray<2> arguments(cx);
  if (!text) return false;
  arguments[0].setString(text); arguments[1].set(callback);
  JS::RootedValue receiver(cx, JS::ObjectValue(*owner.page)), result(cx);
  return JS::Call(cx, receiver, method, arguments, &result);
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject prototype(cx);
  State* owner = WindowOwner(cx, args, &prototype);
  if (!owner) return false;
  const unsigned index = Index(args);
  owner->Observe("Window", kWindowHandlerNames[index], int(ApiAccess::Read));
  {
    JSAutoRealm realm(cx, prototype);
    JS::RootedObject store(cx, &JS::GetReservedSlot(prototype, kWindowHandlerStoreSlot).toObject());
    JS::RootedObject values(cx, &JS::GetReservedSlot(store, kHandlerValues).toObject());
    if (!JS_GetElement(cx, values, index, args.rval())) return false;
  }
  return JS_WrapValue(cx, args.rval());
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject prototype(cx);
  State* owner = WindowOwner(cx, args, &prototype);
  if (!owner) return false;
  const unsigned index = Index(args);
  owner->Observe("Window", kWindowHandlerNames[index], int(ApiAccess::Write));
  JS::RootedValue next(cx, args.get(0).isObject() ? args.get(0) : JS::NullValue());
  {
    JSAutoRealm realm(cx, prototype);
    if (!JS_WrapValue(cx, &next)) return false;
    JS::RootedObject store(cx, &JS::GetReservedSlot(prototype, kWindowHandlerStoreSlot).toObject());
    JS::RootedObject values(cx, &JS::GetReservedSlot(store, kHandlerValues).toObject());
    JS::RootedObject callbacks(cx, &JS::GetReservedSlot(store, kHandlerCallbacks).toObject());
    JS::RootedValue old(cx), callback(cx);
    if (!JS_GetElement(cx, values, index, &old) || !JS_GetElement(cx, callbacks, index, &callback)) return false;
    if (next.isObject() && callback.isNull()) {
      JS::RootedObject function(cx, owner->Function(InvokeWindowHandler, "eventHandler", 1, index));
      if (!function || !JS_SetElement(cx, callbacks, index, function)) return false;
      callback.setObject(*function);
    }
    if (old.isObject() && next.isNull() && !Listen(cx, store, *owner, index, callback, false)) return false;
    if (old.isNull() && next.isObject() && !Listen(cx, store, *owner, index, callback, true)) return false;
    if (!JS_SetElement(cx, values, index, next)) return false;
  }
  args.rval().setUndefined(); return true;
}
bool Save(JSContext* cx, JS::HandleObject store, unsigned slot, JS::HandleObject source,
           const char* key, bool accessor = false, bool setter = false) {
  JS::RootedValue value(cx);
  if (accessor) {
    JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> d(cx);
    if (!JS_GetOwnPropertyDescriptor(cx, source, key, &d) || d.isNothing()) return false;
    value.setObject(*(setter ? d->setter() : d->getter()));
  } else if (!JS_GetProperty(cx, source, key, &value)) return false;
  JS::SetReservedSlot(store, slot, value); return true;
}
}
bool InstallWindowHandlers(State& owner, interaction::Realm& events) {
  JSContext* cx = owner.cx;
  JS::RootedObject prototype(cx), target(cx, events.prototype());
  JS::RootedObject store(cx, JS_NewObjectWithGivenProto(cx, &kHandlerStoreClass, nullptr));
  JS::RootedObject values(cx, JS::NewArrayObject(cx, kWindowHandlerCount));
  JS::RootedObject callbacks(cx, JS::NewArrayObject(cx, kWindowHandlerCount));
  if (!store || !values || !callbacks || !JS_GetPrototype(cx, owner.page, &prototype)) return false;
  JS::SetReservedSlot(store, kHandlerValues, JS::ObjectValue(*values));
  JS::SetReservedSlot(store, kHandlerCallbacks, JS::ObjectValue(*callbacks));
  JS::SetReservedSlot(prototype, kWindowHandlerStoreSlot, JS::ObjectValue(*store));
  if (!Save(cx, store, kHandlerAdd, target, "addEventListener") ||
      !Save(cx, store, kHandlerRemove, target, "removeEventListener")) return false;
  JS::RootedValue value(cx);
  JS::RootedObject constructor(cx), event(cx), error(cx), before(cx);
  for (const auto& row : {std::pair{"Event", &event}, {"ErrorEvent", &error}, {"BeforeUnloadEvent", &before}}) {
    if (!JS_GetProperty(cx, owner.page, row.first, &value) || !value.isObject()) return false;
    constructor.set(&value.toObject());
    if (!JS_GetProperty(cx, constructor, "prototype", &value) || !value.isObject()) return false;
    row.second->set(&value.toObject());
  }
  if (!Save(cx, store, kHandlerPrevent, event, "preventDefault")) return false;
  const char* errorNames[] = {"message", "filename", "lineno", "colno", "error"};
  for (unsigned i = 0; i < 5; ++i)
    if (!Save(cx, store, kHandlerErrorMessage + i, error, errorNames[i], true)) return false;
  if (!Save(cx, store, kHandlerBeforeUnloadGet, before, "returnValue", true) ||
      !Save(cx, store, kHandlerBeforeUnloadSet, before, "returnValue", true, true)) return false;
  for (unsigned i = 0; i < kWindowHandlerCount; ++i) {
    JS::RootedValue empty(cx, JS::NullValue());
    if (!JS_SetElement(cx, values, i, empty) || !JS_SetElement(cx, callbacks, i, empty)) return false;
    if (i >= kDefaultWindowHandlerCount && !owner.LegacyTouchEventsEnabled()) continue;
    if (!owner.Accessor(owner.page, kWindowHandlerNames[i], i, JSPROP_ENUMERATE, Get, Set) ||
        !owner.helpers->Accessor("Window", owner.page, kWindowHandlerNames[i])) return false;
  }
  return true;
}
}
