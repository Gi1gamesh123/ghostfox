// The T9/T10 navigation events: PopStateEvent carries the session-history state
// that a traversal restored and HashChangeEvent carries the old and new
// fragment URLs. Both are direct Event subclasses, so their payload tables live
// here instead of growing event_fields.cpp/event_init.cpp past their split
// budget; event_derived.cpp keeps the one interface table and
// runtime_event.cpp delivers the trusted instances.
#include "interaction/internal.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include <string>

namespace ruyijs::interaction {
namespace {
JSObject* Self(const JS::CallArgs& args) { return &args.thisv().toObject(); }
bool Slot(JSContext* cx, const JS::CallArgs& args, Iface iface, const char* member,
          uint32_t slot) {
  Event* event = Receiver(cx, args, iface);
  if (!event) return false;
  ReportEventAccess(cx, event, InterfaceName(iface), member, int(ApiAccess::Read));
  args.rval().set(JS::GetReservedSlot(Self(args), slot));
  return JS_WrapValue(cx, args.rval());
}
bool GetPopStateState(JSContext* cx, unsigned argc, JS::Value* vp) {
  return Slot(cx, JS::CallArgsFromVp(argc, vp), Iface::PopState, "state",
              kEventPopState);
}
bool GetHashChangeOldURL(JSContext* cx, unsigned argc, JS::Value* vp) {
  return Slot(cx, JS::CallArgsFromVp(argc, vp), Iface::HashChange, "oldURL",
              kEventHashChangeOldUrl);
}
bool GetHashChangeNewURL(JSContext* cx, unsigned argc, JS::Value* vp) {
  return Slot(cx, JS::CallArgsFromVp(argc, vp), Iface::HashChange, "newURL",
              kEventHashChangeNewUrl);
}
}  // namespace

extern const JSPropertySpec kPopStateProperties[] = {
  JS_PSG("state", GetPopStateState, JSPROP_ENUMERATE),
  JS_PS_END
};
extern const JSPropertySpec kHashChangeProperties[] = {
  JS_PSG("oldURL", GetHashChangeOldURL, JSPROP_ENUMERATE),
  JS_PSG("newURL", GetHashChangeNewURL, JSPROP_ENUMERATE),
  JS_PS_END
};

// PopStateEventInit.state is `any` and defaults to null.
bool InitPopState(JSContext* cx, JS::HandleObject dict, JS::HandleObject object) {
  JS::RootedValue state(cx, JS::NullValue());
  if (dict && !ReadInitValue(cx, dict, "state", &state)) return false;
  JS::SetReservedSlot(object, kEventPopState, state);
  return true;
}
// HashChangeEventInit.oldURL/newURL are USVString and default to "".
bool InitHashChange(JSContext* cx, JS::HandleObject dict, JS::HandleObject object) {
  std::string old_url, new_url;
  if (dict && (!ReadInitString(cx, dict, "oldURL", &old_url) ||
               !ReadInitString(cx, dict, "newURL", &new_url))) return false;
  return SetSlotText(cx, object, kEventHashChangeOldUrl, old_url) &&
         SetSlotText(cx, object, kEventHashChangeNewUrl, new_url);
}
}  // namespace ruyijs::interaction