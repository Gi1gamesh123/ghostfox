#include "interaction/internal.h"
#include "js/Object.h"

namespace ruyijs::interaction {
bool IsRtcExtraEvent(Iface iface) {
  return iface == Iface::RtcTone || iface == Iface::RtcErrorPayload || iface == Iface::RtcIceError;
}
namespace {
bool Get(JSContext* cx, const JS::CallArgs& args, Iface iface, const char* name, uint32_t slot) {
  Event* event = Receiver(cx, args, iface, true);
  if (!event) return false;
  ReportEventAccess(cx, event, InterfaceName(iface), name, int(ApiAccess::Read));
  if (iface == Iface::RtcErrorPayload) args.rval().setNull();
  else args.rval().set(JS::GetReservedSlot(event->object, slot));
  return JS_WrapValue(cx, args.rval());
}
#define RTC_EXTRA_GETTER(function, iface, name, slot) \
  bool function(JSContext* cx, unsigned n, JS::Value* vp) { \
    return Get(cx, JS::CallArgsFromVp(n, vp), Iface::iface, name, slot); \
  }
RTC_EXTRA_GETTER(Tone, RtcTone, "tone", kEventPublicFirst)
RTC_EXTRA_GETTER(ErrorPayload, RtcErrorPayload, "error", kEventPublicFirst)
RTC_EXTRA_GETTER(Address, RtcIceError, "address", kEventPublicFirst)
RTC_EXTRA_GETTER(Port, RtcIceError, "port", kEventPublicSecond)
RTC_EXTRA_GETTER(Url, RtcIceError, "url", kEventPublicThird)
RTC_EXTRA_GETTER(ErrorCode, RtcIceError, "errorCode", kEventPublicFourth)
RTC_EXTRA_GETTER(ErrorText, RtcIceError, "errorText", kEventPublicFifth)
#undef RTC_EXTRA_GETTER
}
extern const JSPropertySpec kRtcToneProperties[] = {
  JS_PSG("tone", Tone, JSPROP_ENUMERATE), JS_PS_END};
extern const JSPropertySpec kRtcErrorPayloadProperties[] = {
  JS_PSG("error", ErrorPayload, JSPROP_ENUMERATE), JS_PS_END};
extern const JSPropertySpec kRtcIceErrorProperties[] = {
  JS_PSG("address", Address, JSPROP_ENUMERATE),
  JS_PSG("port", Port, JSPROP_ENUMERATE),
  JS_PSG("url", Url, JSPROP_ENUMERATE),
  JS_PSG("errorCode", ErrorCode, JSPROP_ENUMERATE),
  JS_PSG("errorText", ErrorText, JSPROP_ENUMERATE), JS_PS_END};
}  // namespace ruyijs::interaction
