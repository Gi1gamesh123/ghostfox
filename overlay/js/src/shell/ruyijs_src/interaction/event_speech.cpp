// Speech event payload placeholders, installed only by the original Event owner.
#include "interaction/internal.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::interaction {
namespace {
enum Field { Utterance, CharIndex, CharLength, ElapsedTime, Name, ErrorCode };
const char* const names[] = {"utterance", "charIndex", "charLength", "elapsedTime", "name", "error"};
bool Get(JSContext* cx, const JS::CallArgs& args, Field field) {
  const Iface iface = field == ErrorCode ? Iface::SpeechError : Iface::Speech;
  Event* event = Receiver(cx, args, iface, true);
  if (!event) return false;
  ReportEventAccess(cx, event, InterfaceName(iface), names[field], int(ApiAccess::Read));
  // A runtime event from the Speech Synthesis owner carries its payload.
  const JS::Value payload = JS::GetReservedSlot(event->object, kEventPublicSecond);
  if (payload.isObject() && field != ErrorCode) {
    JS::RootedObject init(cx, &payload.toObject());
    if (!JS_GetProperty(cx, init, names[field], args.rval())) return false;
    return JS_WrapValue(cx, args.rval());
  }
  if (field == Utterance) args.rval().setNull();
  else if (event->kind == Kind::Speech) args.rval().setUndefined();
  else if (field == ErrorCode) args.rval().set(JS::GetReservedSlot(event->object, kEventPublicFirst));
  else if (field == Name) args.rval().setString(JS_GetEmptyString(cx));
  else args.rval().setInt32(0);
  return JS_WrapValue(cx, args.rval());
}
#define SPEECH_GETTER(name, field) \
  bool name(JSContext* cx, unsigned n, JS::Value* vp) { \
    return Get(cx, JS::CallArgsFromVp(n, vp), field); \
  }
SPEECH_GETTER(GetUtterance, Utterance)
SPEECH_GETTER(GetCharIndex, CharIndex)
SPEECH_GETTER(GetCharLength, CharLength)
SPEECH_GETTER(GetElapsedTime, ElapsedTime)
SPEECH_GETTER(GetName, Name)
SPEECH_GETTER(GetError, ErrorCode)
#undef SPEECH_GETTER
}
extern const JSPropertySpec kSpeechProperties[] = {
  JS_PSG("utterance", GetUtterance, JSPROP_ENUMERATE),
  JS_PSG("charIndex", GetCharIndex, JSPROP_ENUMERATE),
  JS_PSG("charLength", GetCharLength, JSPROP_ENUMERATE),
  JS_PSG("elapsedTime", GetElapsedTime, JSPROP_ENUMERATE),
  JS_PSG("name", GetName, JSPROP_ENUMERATE), JS_PS_END};
extern const JSPropertySpec kSpeechErrorProperties[] = {
  JS_PSG("error", GetError, JSPROP_ENUMERATE), JS_PS_END};

bool InitSpeechFields(JSContext* cx, Iface iface, JS::HandleObject dict,
                      JS::HandleObject object) {
  JS::RootedValue value(cx);
  if (iface == Iface::Speech) {
    // Public base construction observes these reads without converting values.
    for (const char* name : {"utterance", "charIndex", "charLength", "elapsedTime", "name", "bubbles"})
      if (!PublicEventRead(cx, dict, name, &value)) return false;
    return !dict || JS_GetProperty(cx, dict, "cancelable", &value);
  }
  if (!PublicEventRead(cx, dict, "error", &value)) return false;
  JS::RootedString error(cx);
  if (!PublicEventString(cx, value, "canceled", &error)) return false;
  JS::SetReservedSlot(object, kEventPublicFirst, JS::StringValue(error));
  return PublicEventRead(cx, dict, "utterance", &value);
}
}
