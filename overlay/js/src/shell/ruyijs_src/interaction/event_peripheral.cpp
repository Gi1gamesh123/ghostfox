// Payload placeholders only; MIDI, cookie storage and PiP sessions have other owners.
#include "interaction/internal.h"
#include "runtime/page_helpers.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"
#include "js/experimental/TypedData.h"

namespace ruyijs::interaction {
bool GetTrustedAcrossRealms(JSContext*, unsigned, JS::Value*);
bool IsPeripheralEvent(Iface iface) {
  return iface == Iface::MidiConnection || iface == Iface::MidiMessage ||
         iface == Iface::CookieChange || iface == Iface::PictureInPicture;
}
namespace {
bool Get(JSContext* cx, const JS::CallArgs& args, Iface iface, const char* name, uint32_t slot) {
  Event* event = Receiver(cx, args, iface, true);
  if (!event) return false;
  ReportEventAccess(cx, event, InterfaceName(iface), name, int(ApiAccess::Read));
  args.rval().setNull();
  if (iface == Iface::MidiMessage) {
    JS::RootedObject array(cx, JS_NewUint8Array(cx, 0));
    if (!array) return false;
    args.rval().setObject(*array);
  }
  if (iface == Iface::CookieChange) {
    // The changed/deleted lists this event carries: the firing path stores them
    // in the public slots, and an empty list stands in when none was given.
    JS::RootedValue value(cx, JS::GetReservedSlot(event->object, slot));
    if (value.isObject()) {
      args.rval().set(value);
      return JS_WrapValue(cx, args.rval());
    }
    JS::RootedObject array(cx, JS::NewArrayObject(cx, 0));
    if (!array) return false;
    args.rval().setObject(*array);
  }
  return true;
}
#define PERIPHERAL_GETTER(function, iface, name, slot) \
  bool function(JSContext* cx, unsigned n, JS::Value* vp) { \
    return Get(cx, JS::CallArgsFromVp(n, vp), Iface::iface, name, slot); \
  }
PERIPHERAL_GETTER(Port, MidiConnection, "port", 0)
PERIPHERAL_GETTER(Data, MidiMessage, "data", 0)
PERIPHERAL_GETTER(Changed, CookieChange, "changed", kEventPublicFirst)
PERIPHERAL_GETTER(Deleted, CookieChange, "deleted", kEventPublicSecond)
PERIPHERAL_GETTER(Window, PictureInPicture, "pictureInPictureWindow", 0)
#undef PERIPHERAL_GETTER
}
extern const JSPropertySpec kMidiConnectionProperties[] = {
  JS_PSG("port", Port, JSPROP_ENUMERATE), JS_PS_END};
extern const JSPropertySpec kMidiMessageProperties[] = {
  JS_PSG("data", Data, JSPROP_ENUMERATE), JS_PS_END};
extern const JSPropertySpec kCookieChangeProperties[] = {
  JS_PSG("changed", Changed, JSPROP_ENUMERATE),
  JS_PSG("deleted", Deleted, JSPROP_ENUMERATE), JS_PS_END};
extern const JSPropertySpec kPictureInPictureProperties[] = {
  JS_PSG("pictureInPictureWindow", Window, JSPROP_ENUMERATE), JS_PS_END};

namespace {
// One CookieChangeEvent list in its public slot: the given sequence, or the empty
// list the getter returns when the dictionary omitted it.
bool CookieList(JSContext* cx, JS::HandleObject object, uint32_t slot, const char* name,
                JS::HandleObject dict) {
  JS::RootedValue value(cx);
  if (!PublicEventRead(cx, dict, name, &value)) return false;
  if (value.isObject()) {
    JS::SetReservedSlot(object, slot, value);
    return true;
  }
  JS::RootedObject array(cx, JS::NewArrayObject(cx, 0));
  if (!array) return false;
  JS::SetReservedSlot(object, slot, JS::ObjectValue(*array));
  return true;
}
}  // namespace
bool InitPeripheralFields(JSContext* cx, Iface iface, JS::HandleObject dict,
                          JS::HandleObject object) {
  JS::RootedValue value(cx);
  if (iface == Iface::CookieChange)
    return object && CookieList(cx, object, kEventPublicFirst, "changed", dict) &&
           CookieList(cx, object, kEventPublicSecond, "deleted", dict);
  if (iface == Iface::PictureInPicture)
    return PublicEventRead(cx, dict, "pictureInPictureWindow", &value);
  return true;
}
bool InstallPictureInPictureTrusted(State& state, JS::HandleObject object) {
  auto& getter = state.pictureInPictureTrustedGetter;
  if (!getter) {
    JSFunction* function = JS_NewFunction(state.cx, GetTrustedAcrossRealms, 0, 0, "get isTrusted");
    if (!function) return false;
    getter = JS_GetFunctionObject(function);
    if (!state.helpers->Member(getter, "PictureInPictureEvent", "get isTrusted", "get"))
      return false;
  }
  JS::RootedObject setter(state.cx);
  return JS_DefineProperty(state.cx, object, "isTrusted", getter, setter,
                           JSPROP_ENUMERATE | JSPROP_PERMANENT);
}
}  // namespace ruyijs::interaction
