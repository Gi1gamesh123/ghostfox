#include "resource_network/xhr.h"
#include "js/CallAndConstruct.h"
#include "js/Object.h"
#include "js/Wrapper.h"
namespace ruyijs::resource_network {
const char* XhrHandlerName(int slot) {
  static const char* names[] = {"onloadstart", "onprogress", "onabort", "onerror",
      "onload", "ontimeout", "onloadend", "onreadystatechange"};
  return names[slot-XLoadStart];
}
bool XhrHandlerGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  int slot = XhrSelector(args);
  JS::RootedObject self(cx);
  auto* record = XhrReceiver(cx, args, &self, slot != XReadyHandler);
  if (!record) return false;
  record->owner->Observe(slot == XReadyHandler ? "XMLHttpRequest" : "XMLHttpRequestEventTarget",
                        XhrHandlerName(slot), int(ApiAccess::Read));
  JS::RootedValue result(cx, JS::GetReservedSlot(self, slot));
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
namespace {
bool Listen(JSContext* cx, JS::HandleObject self, JS::HandleValue method,
            const char* type, JS::HandleValue callback) {
  JS::RootedString name(cx, JS_NewStringCopyZ(cx, type));
  JS::RootedValueArray<2> values(cx);
  JS::RootedValue ignored(cx);
  if (!name) return false;
  values[0].setString(name); values[1].set(callback);
  return JS::Call(cx, self, method, values, &ignored);
}
}
bool XhrHandlerSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  int slot = XhrSelector(args);
  JS::RootedObject self(cx);
  auto* record = XhrReceiver(cx, args, &self, slot != XReadyHandler);
  if (!record) return false;
  auto& s = *record->owner;
  s.Observe(slot == XReadyHandler ? "XMLHttpRequest" : "XMLHttpRequestEventTarget",
            XhrHandlerName(slot), int(ApiAccess::Write));
  JS::RootedValue next(cx, args.get(0)), previous(cx, JS::GetReservedSlot(self, slot));
  JS::RootedValue add(cx, s.xhrAddListener), remove(cx, s.xhrRemoveListener);
  {
    JSAutoRealm realm(cx, self);
    if (!JS_WrapValue(cx, &next) || !JS_WrapValue(cx, &add) || !JS_WrapValue(cx, &remove)) return false;
    const char* type = XhrHandlerName(slot) + 2;
    if (previous.isObject() && !Listen(cx, self, remove, type, previous)) return false;
    if (next.isObject() && JS::IsCallable(&next.toObject())) {
      if (!Listen(cx, self, add, type, next)) return false;
    } else next.setNull();
    JS::SetReservedSlot(self, slot, next);
  }
  args.rval().setUndefined(); return true;
}
}
