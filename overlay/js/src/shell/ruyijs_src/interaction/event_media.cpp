// Event payloads only; media, key sessions and track factories stay with their owners.
#include "interaction/internal.h"
#include "js/ArrayBuffer.h"
#include "js/Object.h"

namespace ruyijs::interaction {
bool IsMediaPayloadEvent(Iface iface) {
  return iface == Iface::MediaEncrypted || iface == Iface::MediaKeyMessage ||
         iface == Iface::MediaStream || iface == Iface::MediaTrack;
}
namespace {
bool Get(JSContext* cx, const JS::CallArgs& args, Iface iface, const char* name, bool second) {
  Event* event = Receiver(cx, args, iface, true);
  if (!event) return false;
  ReportEventAccess(cx, event, InterfaceName(iface), name, int(ApiAccess::Read));
  if (iface == Iface::MediaStream) args.rval().setNull();
  else if (iface == Iface::MediaKeyMessage) {
    if (second) {
      JS::RootedObject buffer(cx, JS::NewArrayBuffer(cx, 0));
      if (!buffer) return false;
      args.rval().setObject(*buffer);
    } else {
      JS::RootedString type(cx, JS_NewStringCopyZ(cx, "license-request"));
      if (!type) return false;
      args.rval().setString(type);
    }
  } else {
    args.rval().set(JS::GetReservedSlot(event->object, second ? kEventPublicSecond : kEventPublicFirst));
  }
  return JS_WrapValue(cx, args.rval());
}
#define MEDIA_GETTER(function, iface, name, second) \
  bool function(JSContext* cx, unsigned n, JS::Value* vp) { \
    return Get(cx, JS::CallArgsFromVp(n, vp), Iface::iface, name, second); \
  }
MEDIA_GETTER(InitDataType, MediaEncrypted, "initDataType", false)
MEDIA_GETTER(InitData, MediaEncrypted, "initData", true)
MEDIA_GETTER(MessageType, MediaKeyMessage, "messageType", false)
MEDIA_GETTER(Message, MediaKeyMessage, "message", true)
MEDIA_GETTER(Stream, MediaStream, "stream", false)
MEDIA_GETTER(Track, MediaTrack, "track", false)
#undef MEDIA_GETTER
}
extern const JSPropertySpec kMediaEncryptedProperties[] = {
  JS_PSG("initDataType", InitDataType, JSPROP_ENUMERATE),
  JS_PSG("initData", InitData, JSPROP_ENUMERATE), JS_PS_END};
extern const JSPropertySpec kMediaKeyMessageProperties[] = {
  JS_PSG("messageType", MessageType, JSPROP_ENUMERATE),
  JS_PSG("message", Message, JSPROP_ENUMERATE), JS_PS_END};
extern const JSPropertySpec kMediaStreamProperties[] = {
  JS_PSG("stream", Stream, JSPROP_ENUMERATE), JS_PS_END};
extern const JSPropertySpec kMediaTrackProperties[] = {
  JS_PSG("track", Track, JSPROP_ENUMERATE), JS_PS_END};

bool InitMediaPayloadFields(JSContext* cx, Iface iface, JS::HandleObject dict,
                            JS::HandleObject object) {
  JS::RootedValue value(cx);
  if (iface == Iface::MediaKeyMessage) return true;
  if (iface == Iface::MediaStream) return PublicEventRead(cx, dict, "stream", &value);
  if (iface == Iface::MediaTrack) {
    if (!PublicEventRead(cx, dict, "track", &value)) return false;
    JS::SetReservedSlot(object, kEventPublicFirst, value.isUndefined() ? JS::NullValue() : value.get());
    return true;
  }
  if (!PublicEventRead(cx, dict, "initDataType", &value)) return false;
  JS::RootedString text(cx, JS_GetEmptyString(cx));
  if (!value.isUndefined() && !PublicEventString(cx, value, nullptr, &text)) return false;
  JS::SetReservedSlot(object, kEventPublicFirst, JS::StringValue(text));
  if (!PublicEventRead(cx, dict, "initData", &value)) return false;
  JS::SetReservedSlot(object, kEventPublicSecond, value.isUndefined() ? JS::NullValue() : value.get());
  return true;
}
}  // namespace ruyijs::interaction
