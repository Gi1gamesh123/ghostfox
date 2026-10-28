#include "interaction/event_keyboard.h"
#include "interaction/event_legacy_conversion.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/Realm.h"

namespace ruyijs::interaction {
bool InitKeyboardEvent(JSContext* cx, unsigned argc, JS::Value* vp) {
  const auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::Keyboard, true);
  if (!event) return false;
  ReportEventAccess(cx, event, "KeyboardEvent", "initKeyboardEvent", int(ApiAccess::Call));
  if (!argc) return Error(cx, "TypeError", "initKeyboardEvent requires a type");
  JS::RootedString type(cx);
  std::string dispatch_key;
  JS::RootedValue view(cx), key(cx);
  uint32_t location = 0;
  // Complete WebIDL conversion before committing. Reentrant changes made by
  // conversions survive failures; this method never publishes partial fields.
  if (!Text(cx, args[0], &type, &dispatch_key) || !KeyboardView(cx, args.get(3), &view) ||
      !KeyboardText(cx, args.get(4), &key) ||
      (args.hasDefined(5) && !JS::ToUint32(cx, args[5], &location))) return false;
  args.rval().setUndefined();
  if (event->value.dispatching()) return true;
  JS::RootedObject object(cx, event->object);
  JS::RootedValue type_value(cx, JS::StringValue(type));
  JSAutoRealm realm(cx, object);
  if (!JS_WrapValue(cx, &view) || !JS_WrapValue(cx, &key) || !JS_WrapValue(cx, &type_value)) return false;
  event->type = type_value.toString();
  event->value.initialize(domain::EventType(std::move(dispatch_key)),
      JS::ToBoolean(args.get(1)), JS::ToBoolean(args.get(2)));
  event->value.set_composed(KeyboardMessageOf(event) != domain::KeyboardMessage::Other);
  SetSlotInt(object, kEventUiDetail, 0);
  JS::SetReservedSlot(object, kEventView, view);
  JS::SetReservedSlot(object, kEventKey, key);
  SetSlotNumber(object, kEventLocation, location);
  SetSlotInt(object, kEventModifiers, LegacyModifiers(args, 6));
  SetSlotInt(object, kEventKeyboardFlags,
      JS::GetReservedSlot(object, kEventKeyboardFlags).toInt32() & ~kKeyboardConstructed);
  return true;
}
}
