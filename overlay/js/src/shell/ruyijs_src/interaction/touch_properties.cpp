#include "interaction/touch.h"
#include "js/Object.h"
#include "js/Realm.h"

namespace ruyijs::interaction {
namespace {
bool Get(JSContext* cx, unsigned argc, JS::Value* vp, unsigned slot, const char* name) {
  const auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject object(cx, TouchReceiver(cx, args.thisv()));
  if (!object) return false;
  State* state = RealmState(cx);
  if (state) state->Observe("Touch", name, int(ApiAccess::Read));
  args.rval().set(JS::GetReservedSlot(object, slot));
  return JS_WrapValue(cx, args.rval());
}
#define TOUCH_GETTER(fn, slot, name) \
  bool fn(JSContext* cx, unsigned argc, JS::Value* vp) { return Get(cx, argc, vp, slot, name); }
TOUCH_GETTER(Identifier, kTouchIdentifier, "identifier")
TOUCH_GETTER(TargetValue, kTouchTarget, "target")
TOUCH_GETTER(ScreenX, kTouchScreenX, "screenX")
TOUCH_GETTER(ScreenY, kTouchScreenY, "screenY")
TOUCH_GETTER(ClientX, kTouchClientX, "clientX")
TOUCH_GETTER(ClientY, kTouchClientY, "clientY")
TOUCH_GETTER(PageX, kTouchPageX, "pageX")
TOUCH_GETTER(PageY, kTouchPageY, "pageY")
TOUCH_GETTER(RadiusX, kTouchRadiusX, "radiusX")
TOUCH_GETTER(RadiusY, kTouchRadiusY, "radiusY")
TOUCH_GETTER(Rotation, kTouchRotation, "rotationAngle")
TOUCH_GETTER(Force, kTouchForce, "force")
#undef TOUCH_GETTER
}
extern const JSPropertySpec kTouchValueProperties[] = {
  JS_PSG("identifier", Identifier, JSPROP_ENUMERATE), JS_PSG("target", TargetValue, JSPROP_ENUMERATE),
  JS_PSG("screenX", ScreenX, JSPROP_ENUMERATE), JS_PSG("screenY", ScreenY, JSPROP_ENUMERATE),
  JS_PSG("clientX", ClientX, JSPROP_ENUMERATE), JS_PSG("clientY", ClientY, JSPROP_ENUMERATE),
  JS_PSG("pageX", PageX, JSPROP_ENUMERATE), JS_PSG("pageY", PageY, JSPROP_ENUMERATE),
  JS_PSG("radiusX", RadiusX, JSPROP_ENUMERATE), JS_PSG("radiusY", RadiusY, JSPROP_ENUMERATE),
  JS_PSG("rotationAngle", Rotation, JSPROP_ENUMERATE), JS_PSG("force", Force, JSPROP_ENUMERATE),
  JS_PS_END
};
}
