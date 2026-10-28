#include "interaction/internal.h"
#include "js/Object.h"

namespace ruyijs::interaction {
namespace {
bool InitCustom(JSContext* cx, unsigned argc, JS::Value* vp) {
  const auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::Custom);
  if (!event) return false;
  ReportEventAccess(cx, event, "CustomEvent", "initCustomEvent", int(ApiAccess::Call));
  if (!argc) return Error(cx, "TypeError", "initCustomEvent requires a type");
  args.rval().setUndefined();
  if (event->value.dispatching()) return true;
  if (!LegacyBase(cx, event, args)) return false;
  JS::SetReservedSlot(event->object, kEventCustomDetail,
      args.hasDefined(3) ? args[3].get() : JS::NullValue());
  return true;
}
bool InitText(JSContext* cx, unsigned argc, JS::Value* vp) {
  const auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::TextInput);
  if (!event) return false;
  ReportEventAccess(cx, event, "TextEvent", "initTextEvent", int(ApiAccess::Call));
  if (!argc) return Error(cx, "TypeError", "initTextEvent requires a type");
  args.rval().setUndefined();
  if (event->value.dispatching()) return true;
  if (!LegacyBase(cx, event, args)) return false;
  JS::RootedObject object(cx, event->object);
  JS::RootedValue view(cx);
  std::string data = "undefined";
  if (!LegacyObject(cx, args, 3, &view) || !LegacyString(cx, args, 4, &data)) return false;
  SetSlotInt(object, kEventUiDetail, 0);
  JS::SetReservedSlot(object, kEventView, view);
  return SetSlotText(cx, object, kEventPublicFirst, data);
}
}
extern const JSFunctionSpec kCustomMethods[] = {
  JS_FN("initCustomEvent", InitCustom, 1, JSPROP_ENUMERATE), JS_FS_END};
extern const JSFunctionSpec kTextInputMethods[] = {
  JS_FN("initTextEvent", InitText, 5, JSPROP_ENUMERATE), JS_FS_END};
}  // namespace ruyijs::interaction
