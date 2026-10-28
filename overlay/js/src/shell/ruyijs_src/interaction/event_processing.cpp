// Event wrappers only; processing, font, GPU and scheduler services keep their owners.
#include "interaction/internal.h"
#include "js/Array.h"
#include "js/Object.h"

namespace ruyijs::interaction {
bool IsProcessingEvent(Iface iface) {
  return iface == Iface::AudioProcessing || iface == Iface::FontLoad ||
         iface == Iface::GpuUncaptured || iface == Iface::TaskPriority;
}
namespace {
bool Get(JSContext* cx, const JS::CallArgs& args, Iface iface, const char* name, uint32_t slot) {
  Event* event = Receiver(cx, args, iface, true);
  if (!event) return false;
  ReportEventAccess(cx, event, InterfaceName(iface), name, int(ApiAccess::Read));
  if (iface == Iface::GpuUncaptured) args.rval().setNull();
  else if (iface == Iface::FontLoad) {
    JS::RootedObject array(cx, JS::NewArrayObject(cx, 0));
    if (!array) return false;
    args.rval().setObject(*array);
  } else args.rval().set(JS::GetReservedSlot(event->object, slot));
  return JS_WrapValue(cx, args.rval());
}
#define PROCESSING_GETTER(function, iface, name, slot) \
  bool function(JSContext* cx, unsigned n, JS::Value* vp) { \
    return Get(cx, JS::CallArgsFromVp(n, vp), Iface::iface, name, slot); \
  }
PROCESSING_GETTER(PlaybackTime, AudioProcessing, "playbackTime", kEventPublicFirst)
PROCESSING_GETTER(InputBuffer, AudioProcessing, "inputBuffer", kEventPublicSecond)
PROCESSING_GETTER(OutputBuffer, AudioProcessing, "outputBuffer", kEventPublicThird)
PROCESSING_GETTER(FontFaces, FontLoad, "fontfaces", kEventPublicFirst)
PROCESSING_GETTER(GpuError, GpuUncaptured, "error", kEventPublicFirst)
PROCESSING_GETTER(PreviousPriority, TaskPriority, "previousPriority", kEventPublicFirst)
#undef PROCESSING_GETTER
}
extern const JSPropertySpec kAudioProcessingProperties[] = {
  JS_PSG("playbackTime", PlaybackTime, JSPROP_ENUMERATE),
  JS_PSG("inputBuffer", InputBuffer, JSPROP_ENUMERATE),
  JS_PSG("outputBuffer", OutputBuffer, JSPROP_ENUMERATE), JS_PS_END};
extern const JSPropertySpec kFontLoadProperties[] = {
  JS_PSG("fontfaces", FontFaces, JSPROP_ENUMERATE), JS_PS_END};
extern const JSPropertySpec kGpuUncapturedProperties[] = {
  JS_PSG("error", GpuError, JSPROP_ENUMERATE), JS_PS_END};
extern const JSPropertySpec kTaskPriorityProperties[] = {
  JS_PSG("previousPriority", PreviousPriority, JSPROP_ENUMERATE), JS_PS_END};
}  // namespace ruyijs::interaction
