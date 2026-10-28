// Payload storage on the existing Event object's traced slots. No subsystem
// factory or business state is introduced by these event-only interfaces.
#include "interaction/internal.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::interaction {
namespace {
bool Get(JSContext* cx, const JS::CallArgs& args, Iface iface,
         const char* name, uint32_t slot) {
  Event* event = Receiver(cx, args, iface);
  if (!event) return false;
  ReportEventAccess(cx, event, InterfaceName(iface), name, int(ApiAccess::Read));
  args.rval().set(JS::GetReservedSlot(event->object, slot));
  return JS_WrapValue(cx, args.rval());
}
#define PAYLOAD_GETTER(function, iface, member, slot) \
  bool function(JSContext* cx, unsigned argc, JS::Value* vp) { \
    return Get(cx, JS::CallArgsFromVp(argc, vp), Iface::iface, member, slot); \
  }
PAYLOAD_GETTER(CurrentTime, AnimationPlayback, "currentTime", kEventPublicFirst)
PAYLOAD_GETTER(TimelineTime, AnimationPlayback, "timelineTime", kEventPublicSecond)
PAYLOAD_GETTER(Data, BlobPayload, "data", kEventPublicFirst)
PAYLOAD_GETTER(Skipped, ContentVisibility, "skipped", kEventPublicFirst)
PAYLOAD_GETTER(FormData, FormDataPayload, "formData", kEventPublicFirst)
PAYLOAD_GETTER(OldState, Toggle, "oldState", kEventPublicFirst)
PAYLOAD_GETTER(NewState, Toggle, "newState", kEventPublicSecond)
PAYLOAD_GETTER(Gamepad, Gamepad, "gamepad", kEventPublicFirst)
PAYLOAD_GETTER(TextData, TextInput, "data", kEventPublicFirst)
#undef PAYLOAD_GETTER

bool Number(JSContext* cx, JS::HandleObject dict, JS::HandleObject object,
            const char* name, uint32_t slot) {
  JS::RootedValue value(cx);
  if (!PublicEventRead(cx, dict, name, &value)) return false;
  // Public ruyijs accepts only numbers here, without ToNumber coercion.
  JS::SetReservedSlot(object, slot, value.isNumber() ? value.get() : JS::NullValue());
  return true;
}
bool String(JSContext* cx, JS::HandleObject dict, JS::HandleObject object,
            const char* name, uint32_t slot, bool strict_read = false) {
  JS::RootedValue value(cx);
  if (!(strict_read && dict ? JS_GetProperty(cx, dict, name, &value)
                            : PublicEventRead(cx, dict, name, &value))) return false;
  JS::RootedString text(cx, JS_GetEmptyString(cx));
  if (!value.isUndefined()) {
    text = JS::ToString(cx, value);
    if (!text) {
      if (!JS_IsExceptionPending(cx) || JS_IsThrowingOutOfMemory(cx)) return false;
      JS_ClearPendingException(cx);
      text = JS_GetEmptyString(cx);
    }
  }
  JS::SetReservedSlot(object, slot, JS::StringValue(text));
  return true;
}
}

extern const JSPropertySpec kAnimationPlaybackProperties[] = {
  JS_PSG("currentTime", CurrentTime, JSPROP_ENUMERATE),
  JS_PSG("timelineTime", TimelineTime, JSPROP_ENUMERATE), JS_PS_END};
extern const JSPropertySpec kBlobPayloadProperties[] = {
  JS_PSG("data", Data, JSPROP_ENUMERATE), JS_PS_END};
extern const JSPropertySpec kContentVisibilityProperties[] = {
  JS_PSG("skipped", Skipped, JSPROP_ENUMERATE), JS_PS_END};
extern const JSPropertySpec kFormDataPayloadProperties[] = {
  JS_PSG("formData", FormData, JSPROP_ENUMERATE), JS_PS_END};
extern const JSPropertySpec kToggleProperties[] = {
  JS_PSG("oldState", OldState, JSPROP_ENUMERATE),
  JS_PSG("newState", NewState, JSPROP_ENUMERATE), JS_PS_END};
extern const JSPropertySpec kGamepadProperties[] = {
  JS_PSG("gamepad", Gamepad, JSPROP_ENUMERATE), JS_PS_END};
extern const JSPropertySpec kTextInputProperties[] = {
  JS_PSG("data", TextData, JSPROP_ENUMERATE), JS_PS_END};

bool InitPublicEventFields(JSContext* cx, Iface iface, JS::HandleObject dict,
                           JS::HandleObject object) {
  if (iface == Iface::AnimationPlayback)
    return Number(cx, dict, object, "currentTime", kEventPublicFirst) &&
           Number(cx, dict, object, "timelineTime", kEventPublicSecond);
  if (iface == Iface::Toggle)
    return String(cx, dict, object, "oldState", kEventPublicFirst) &&
           String(cx, dict, object, "newState", kEventPublicSecond);
  JS::RootedValue value(cx);
  if (iface == Iface::TextInput) {
    // Same UI payload slots, in the public constructor's observable read order.
    if (dict && !JS_GetProperty(cx, dict, "view", &value)) return false;
    JS::SetReservedSlot(object, kEventView,
                        value.isUndefined() ? JS::NullValue() : value.get());
    value.setUndefined();
    if (dict && !JS_GetProperty(cx, dict, "detail", &value)) return false;
    int32_t detail = 0;
    if (!value.isUndefined() && !JS::ToInt32(cx, value, &detail)) {
      if (!JS_IsExceptionPending(cx) || JS_IsThrowingOutOfMemory(cx)) return false;
      JS_ClearPendingException(cx);
      detail = 0;
    }
    SetSlotInt(object, kEventUiDetail, detail);
    return String(cx, dict, object, "data", kEventPublicFirst, true);
  }
  if (iface == Iface::BlobPayload) {
    if (!JS_GetProperty(cx, dict, "data", &value)) {
      if (!JS_IsExceptionPending(cx) || JS_IsThrowingOutOfMemory(cx)) return false;
      JS_ClearPendingException(cx);
      return Error(cx, "TypeError", "BlobEvent data could not be read");
    }
    JS::SetReservedSlot(object, kEventPublicFirst,
                        value.isUndefined() ? JS::NullValue() : value.get());
    return true;
  }
  const bool skipped = iface == Iface::ContentVisibility;
  const char* member = skipped ? "skipped" : iface == Iface::Gamepad ? "gamepad" : "formData";
  if (!PublicEventRead(cx, dict, member, &value)) return false;
  JS::SetReservedSlot(object, kEventPublicFirst,
                      skipped ? JS::BooleanValue(JS::ToBoolean(value)) : JS::NullValue());
  return true;
}
}  // namespace ruyijs::interaction
