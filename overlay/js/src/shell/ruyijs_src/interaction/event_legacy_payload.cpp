#include "interaction/event_legacy_conversion.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/Wrapper.h"

namespace ruyijs::interaction {
namespace {
bool InitMessageEvent(JSContext* cx, unsigned argc, JS::Value* vp) {
  const auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::Message);
  if (!event) return false;
  ReportEventAccess(cx, event, "MessageEvent", "initMessageEvent", int(ApiAccess::Call));
  if (!args.get(0).isString())
    return Error(cx, "TypeError", "initMessageEvent requires a string type");
  args.rval().setUndefined();
  if (event->value.dispatching()) return true;
  JS::RootedString type(cx);
  std::string key;
  if (!Text(cx, args[0], &type, &key)) return false;
  JS::RootedObject ports(cx, JS::NewArrayObject(cx, 0));
  if (!ports) return false;
  if (!JS_FreezeObject(cx, ports)) return false;
  event->type.set(type);
  event->value.initialize(domain::EventType(std::move(key)),
      JS::ToBoolean(args.get(1)), JS::ToBoolean(args.get(2)));
  JS::RootedObject object(cx, event->object);
  JS::SetReservedSlot(object, kEventMessageData, LegacyValue(args, 3));
  JS::SetReservedSlot(object, kEventOrigin, args.get(4).isString()
      ? args[4].get() : JS::StringValue(JS_GetEmptyString(cx)));
  JS::SetReservedSlot(object, kEventLastEventId, args.get(5).isString()
      ? args[5].get() : JS::StringValue(JS_GetEmptyString(cx)));
  JS::SetReservedSlot(object, kEventMessageSource, LegacyTarget(cx, args.get(6)));
  JS::SetReservedSlot(object, kEventPorts, JS::ObjectValue(*ports));
  return true;
}
bool InitStorageEvent(JSContext* cx, unsigned argc, JS::Value* vp) {
  const auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::Storage);
  if (!event) return false;
  ReportEventAccess(cx, event, "StorageEvent", "initStorageEvent", int(ApiAccess::Call));
  if (!argc) return Error(cx, "TypeError", "initStorageEvent requires a type");
  args.rval().setUndefined();
  LegacyConversion conversion(cx);
  JS::RootedValue key(cx, JS::NullValue()), old_value(cx, JS::NullValue()),
      new_value(cx, JS::NullValue()), url(cx, JS::StringValue(JS_GetEmptyString(cx)));
  if (!conversion.Type(args[0]) ||
      (!args.get(3).isNullOrUndefined() && !conversion.String(args[3], &key)) ||
      (!args.get(4).isNullOrUndefined() && !conversion.String(args[4], &old_value)) ||
      (!args.get(5).isNullOrUndefined() && !conversion.String(args[5], &new_value)) ||
      (args.hasDefined(6) && !conversion.String(args[6], &url, true))) return false;
  JS::RootedValue area(cx, JS::NullValue());
  State* owner = Owner(cx, event->object, true);
  if (args.get(7).isObject() && owner->storageBrand) {
    JSObject* candidate = js::CheckedUnwrapStatic(&args[7].toObject());
    if (candidate && owner->storageBrand(candidate)) area.set(args[7]);
  }
  conversion.Commit(event, args);
  JS::RootedObject object(cx, event->object);
  JS::SetReservedSlot(object, kEventStorageKey, key);
  JS::SetReservedSlot(object, kEventStorageOldValue, old_value);
  JS::SetReservedSlot(object, kEventStorageNewValue, new_value);
  JS::SetReservedSlot(object, kEventStorageUrl, url);
  JS::SetReservedSlot(object, kEventStorageArea, area);
  return conversion.Finish();
}
}
extern const JSFunctionSpec kMessageMethods[] = {
  JS_FN("initMessageEvent", InitMessageEvent, 1, JSPROP_ENUMERATE), JS_FS_END};
extern const JSFunctionSpec kStorageMethods[] = {
  JS_FN("initStorageEvent", InitStorageEvent, 1, JSPROP_ENUMERATE), JS_FS_END};
}
