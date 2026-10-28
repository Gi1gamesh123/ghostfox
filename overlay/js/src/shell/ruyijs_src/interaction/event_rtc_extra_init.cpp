#include "interaction/internal.h"
#include "js/Object.h"

namespace ruyijs::interaction {
namespace {
bool TextField(JSContext* cx, JS::HandleObject dict, JS::HandleObject object,
               const char* name, uint32_t slot, bool nullable = false) {
  JS::RootedValue value(cx);
  if (!PublicEventRead(cx, dict, name, &value)) return false;
  if (nullable && value.isNullOrUndefined()) {
    JS::SetReservedSlot(object, slot, JS::NullValue());
    return true;
  }
  JS::RootedString text(cx, JS_GetEmptyString(cx));
  if (!value.isUndefined() && !PublicEventString(cx, value, nullptr, &text)) return false;
  JS::SetReservedSlot(object, slot, JS::StringValue(text));
  return true;
}
bool NumberField(JSContext* cx, JS::HandleObject dict, JS::HandleObject object,
                 const char* name, uint32_t slot, bool nullable = false) {
  JS::RootedValue value(cx);
  if (!PublicEventRead(cx, dict, name, &value)) return false;
  if (nullable && value.isNullOrUndefined()) {
    JS::SetReservedSlot(object, slot, JS::NullValue());
    return true;
  }
  uint16_t number;
  if (!RtcEventUnsignedShort(cx, value, &number)) return false;
  JS::SetReservedSlot(object, slot, JS::Int32Value(number));
  return true;
}
}
bool InitRtcExtraFields(JSContext* cx, Iface iface, JS::HandleObject dict, JS::HandleObject object) {
  if (iface == Iface::RtcTone) return TextField(cx, dict, object, "tone", kEventPublicFirst);
  if (iface == Iface::RtcErrorPayload) {
    JS::RootedValue ignored(cx);
    return PublicEventRead(cx, dict, "error", &ignored);
  }
  return TextField(cx, dict, object, "address", kEventPublicFirst, true) &&
         NumberField(cx, dict, object, "port", kEventPublicSecond, true) &&
         TextField(cx, dict, object, "url", kEventPublicThird) &&
         NumberField(cx, dict, object, "errorCode", kEventPublicFourth) &&
         TextField(cx, dict, object, "errorText", kEventPublicFifth);
}
}  // namespace ruyijs::interaction
