// The T13c Web Audio completion event. OfflineAudioCompletionEvent carries the
// AudioBuffer an OfflineAudioContext render produced, so the fingerprint
// libraries that read `event.renderedBuffer.getChannelData(0)` work. It is a
// direct Event subclass, so its payload table lives here instead of growing
// event_fields.cpp/event_init.cpp past their split budget; the Web Audio owner
// hands the rendered buffer over and delivery stays on the one dispatch tree.
#include "interaction/internal.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::interaction {
namespace {
JSObject* Self(const JS::CallArgs& args) { return &args.thisv().toObject(); }
bool GetRenderedBuffer(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::OfflineAudioComplete);
  if (!event) return false;
  ReportEventAccess(cx, event, InterfaceName(Iface::OfflineAudioComplete),
                    "renderedBuffer", int(ApiAccess::Read));
  args.rval().set(JS::GetReservedSlot(Self(args), kEventRenderedBuffer));
  return JS_WrapValue(cx, args.rval());
}
}  // namespace

extern const JSPropertySpec kOfflineAudioCompleteProperties[] = {
  JS_PSG("renderedBuffer", GetRenderedBuffer, JSPROP_ENUMERATE),
  JS_PS_END
};

// OfflineAudioCompletionEventInit.renderedBuffer is a required AudioBuffer, so
// a missing or non-object member is the WebIDL required-member TypeError. The
// AudioBuffer brand itself belongs to the Web Audio owner; this event interface
// only carries the value it was handed.
bool InitOfflineAudioComplete(JSContext* cx, JS::HandleObject dict,
                              JS::HandleObject object) {
  JS::RootedValue buffer(cx, JS::NullValue());
  if (dict && !ReadInitValue(cx, dict, "renderedBuffer", &buffer)) return false;
  if (!buffer.isObject())
    return Error(cx, "TypeError",
                 "OfflineAudioCompletionEventInit.renderedBuffer is required");
  JS::SetReservedSlot(object, kEventRenderedBuffer, buffer);
  return true;
}
}  // namespace ruyijs::interaction
