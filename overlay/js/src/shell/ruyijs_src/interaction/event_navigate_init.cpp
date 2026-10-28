// Navigation payload only. The destination is the reference the Navigation owner
// (browser_environment) or the page supplied; no session history or AbortSignal service.
#include "interaction/internal.h"
#include "js/Conversions.h"
#include "js/Object.h"

namespace ruyijs::interaction {
bool InitNavigateFields(JSContext* cx, JS::HandleObject dict, JS::HandleObject object) {
  JS::RootedValue value(cx);
  if (!PublicEventRead(cx, dict, "navigationType", &value)) return false;
  JS::RootedString type(cx);
  if (!PublicEventString(cx, value, "push", &type)) return false;
  JS::SetReservedSlot(object, kEventPublicFirst, JS::StringValue(type));
  if (!PublicEventRead(cx, dict, "destination", &value)) return false;
  JS::SetReservedSlot(object, kEventPublicFifth, value.isObject() ? value.get() : JS::NullValue());
  int32_t flags = 0;
  if (!PublicEventRead(cx, dict, "canIntercept", &value)) return false;
  if (value.isUndefined() || JS::ToBoolean(value)) flags |= 1;
  if (!PublicEventRead(cx, dict, "userInitiated", &value)) return false;
  if (JS::ToBoolean(value)) flags |= 2;
  if (!PublicEventRead(cx, dict, "hashChange", &value)) return false;
  if (JS::ToBoolean(value)) flags |= 4;
  JS::SetReservedSlot(object, kEventPublicFourth, JS::Int32Value(flags));
  if (!PublicEventRead(cx, dict, "signal", &value) ||
      !PublicEventRead(cx, dict, "formData", &value)) return false;
  bool failed = false;
  if (!PublicEventRead(cx, dict, "downloadRequest", &value, &failed)) return false;
  JS::SetReservedSlot(object, kEventPublicSecond, failed ? JS::NullValue() : value.get());
  if (!PublicEventRead(cx, dict, "info", &value)) return false;
  JS::SetReservedSlot(object, kEventPublicThird, value);
  return true;
}
}  // namespace ruyijs::interaction
