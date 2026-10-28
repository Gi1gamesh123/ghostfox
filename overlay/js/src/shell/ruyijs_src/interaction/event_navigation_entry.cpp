// Navigation entry-change payload only. Session-history state and entry
// factories stay with their own owner; from holds the supplied reference.
#include "interaction/internal.h"
#include "js/Object.h"

namespace ruyijs::interaction {
namespace {
bool Get(JSContext* cx, const JS::CallArgs& args, const char* name, uint32_t slot) {
  Event* event = Receiver(cx, args, Iface::NavigationEntryChange, true);
  if (!event) return false;
  ReportEventAccess(cx, event, "NavigationCurrentEntryChangeEvent", name, int(ApiAccess::Read));
  args.rval().set(JS::GetReservedSlot(event->object, slot));
  return JS_WrapValue(cx, args.rval());
}
bool NavigationType(JSContext* cx, unsigned argc, JS::Value* vp) {
  return Get(cx, JS::CallArgsFromVp(argc, vp), "navigationType", kEventPublicFirst);
}
bool From(JSContext* cx, unsigned argc, JS::Value* vp) {
  return Get(cx, JS::CallArgsFromVp(argc, vp), "from", kEventPublicSecond);
}
}
extern const JSPropertySpec kNavigationEntryChangeProperties[] = {
  JS_PSG("navigationType", NavigationType, JSPROP_ENUMERATE),
  JS_PSG("from", From, JSPROP_ENUMERATE), JS_PS_END};

bool InitNavigationEntryChange(JSContext* cx, JS::HandleObject dict, JS::HandleObject object) {
  JS::RootedValue value(cx);
  if (!PublicEventRead(cx, dict, "navigationType", &value)) return false;
  if (value.isNullOrUndefined()) {
    JS::SetReservedSlot(object, kEventPublicFirst, JS::NullValue());
  } else {
    JS::RootedString text(cx);
    std::string unused;
    if (!PublicEventType(cx, value, &text, &unused)) return false;
    JS::SetReservedSlot(object, kEventPublicFirst, JS::StringValue(text));
  }
  if (!PublicEventRead(cx, dict, "from", &value)) return false;
  JS::SetReservedSlot(object, kEventPublicSecond, value.isObject() ? value.get() : JS::NullValue());
  return true;
}
}
