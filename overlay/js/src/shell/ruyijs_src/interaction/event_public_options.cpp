// Public ruyijs construction policy for the Firefox-supported event additions.
// Existing event constructors continue to use ReadEventBase and Text.
#include "interaction/internal.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::interaction {
bool IsPublicEvent(Iface iface) {
  return iface == Iface::AnimationPlayback || iface == Iface::BlobPayload ||
         iface == Iface::ContentVisibility || iface == Iface::FormDataPayload ||
         iface == Iface::Toggle || iface == Iface::Gamepad || iface == Iface::TextInput ||
         iface == Iface::NavigationEntryChange || iface == Iface::Speech || iface == Iface::SpeechError ||
         IsPeripheralEvent(iface) || iface == Iface::Navigate || IsMediaPayloadEvent(iface) ||
         IsRtcExtraEvent(iface) || IsProcessingEvent(iface) || iface == Iface::IdbVersionChange ||
         iface == Iface::DocumentPicture;
}
unsigned PublicEventArity(Iface iface) {
  if (iface == Iface::TimeEvent || iface == Iface::MouseScroll || iface == Iface::ScrollArea) return 0;
  if (iface == Iface::MediaStreamTrack || iface == Iface::MediaRecorderError || iface == Iface::Fetch) return 2;
  return iface == Iface::BlobPayload || iface == Iface::FormDataPayload ||
         iface == Iface::NavigationEntryChange || iface == Iface::Speech || iface == Iface::SpeechError ||
         iface == Iface::PictureInPicture || iface == Iface::Navigate ||
         iface == Iface::MediaKeyMessage || IsRtcExtraEvent(iface) ||
         iface == Iface::AudioProcessing || iface == Iface::GpuUncaptured ||
         iface == Iface::DocumentPicture || iface == Iface::TaskPriority ||
         iface == Iface::RtcTransform ? 2 : 1;
}
bool CallPublicEvent(JSContext* cx, const JS::CallArgs& args, Iface iface) {
  if (iface == Iface::TaskPriority)return Error(cx,"TypeError","Event constructor requires new");
  if (iface == Iface::AudioProcessing || iface == Iface::TaskPriority)
    return CallProcessingEvent(cx, args, iface);
  if (iface == Iface::ContentVisibility || iface == Iface::FormDataPayload ||
      iface == Iface::Gamepad || iface == Iface::NavigationEntryChange ||
      iface == Iface::Speech || iface == Iface::SpeechError || iface == Iface::MidiConnection ||
      iface == Iface::MidiMessage || iface == Iface::CookieChange ||
      iface == Iface::MediaEncrypted || iface == Iface::MediaKeyMessage ||
      iface == Iface::FontLoad || iface == Iface::GpuUncaptured || iface == Iface::IdbVersionChange)
    return Error(cx, "TypeError", "Event constructor requires new");
  args.rval().setUndefined();
  return true;
}
namespace {
bool ClearConversion(JSContext* cx) {
  if (!JS_IsExceptionPending(cx) || JS_IsThrowingOutOfMemory(cx)) return false;
  JS_ClearPendingException(cx);
  return true;
}
}
bool PublicEventType(JSContext* cx, JS::HandleValue input,
                     JS::MutableHandleString type, std::string* key) {
  type.set(JS::ToString(cx, input));
  if (!type) {
    if (!ClearConversion(cx)) return false;
    type.set(JS_GetEmptyString(cx));
  }
  JS::RootedValue value(cx, JS::StringValue(type));
  return Text(cx, value, type, key);
}
bool PublicEventRead(JSContext* cx, JS::HandleObject dict, const char* name,
                     JS::MutableHandleValue value, bool* failed) {
  if (failed) *failed = false;
  value.setUndefined();
  if (!dict || JS_GetProperty(cx, dict, name, value)) return true;
  if (!ClearConversion(cx)) return false;
  if (failed) *failed = true;
  value.setUndefined();
  return true;
}
bool ReadPublicEventBase(JSContext* cx, Iface iface, JS::HandleValue options,
                         bool* bubbles, bool* cancelable, bool* composed) {
  if(iface==Iface::TaskPriority)return ReadEventBase(cx,options,bubbles,cancelable,composed);
  *bubbles = *cancelable = *composed = false;
  if (iface == Iface::NavigationEntryChange || iface == Iface::Speech) return true;
  if (iface == Iface::MediaEncrypted || iface == Iface::MediaKeyMessage) return true;
  if (iface == Iface::FontLoad || iface == Iface::GpuUncaptured) return true;
  if (iface == Iface::IdbVersionChange) return true;
  if (iface == Iface::MidiConnection || iface == Iface::MidiMessage || iface == Iface::CookieChange)
    return true;
  if (iface == Iface::SpeechError)
    return options.isObject() || Error(cx, "TypeError", "Speech error options require an object");
  if (iface == Iface::ContentVisibility || iface == Iface::FormDataPayload ||
      iface == Iface::Gamepad)
    return true;
  if (options.isUndefined() && iface != Iface::BlobPayload && iface != Iface::PictureInPicture &&
      iface != Iface::Navigate && iface != Iface::AudioProcessing && !IsRtcExtraEvent(iface))
    return true;
  if (!options.isObject()) return Error(cx, "TypeError", "Event options require an object");
  JS::RootedObject dict(cx, &options.toObject());
  if (iface == Iface::TextInput) {
    bool ignored = false;
    return ReadInitBool(cx, dict, "bubbles", bubbles) &&
           ReadInitBool(cx, dict, "cancelable", cancelable) &&
           ReadInitBool(cx, dict, "composed", &ignored);
  }
  JS::RootedValue value(cx);
  if (!PublicEventRead(cx, dict, "bubbles", &value)) return false;
  *bubbles = JS::ToBoolean(value);
  if (!PublicEventRead(cx, dict, "cancelable", &value)) return false;
  *cancelable = JS::ToBoolean(value);
  if (iface == Iface::PictureInPicture || iface == Iface::Navigate || iface == Iface::MediaStream ||
      IsRtcExtraEvent(iface)) {
    if (!PublicEventRead(cx, dict, "composed", &value)) return false;
    *composed = JS::ToBoolean(value);
  }
  return true;
}
}  // namespace ruyijs::interaction
