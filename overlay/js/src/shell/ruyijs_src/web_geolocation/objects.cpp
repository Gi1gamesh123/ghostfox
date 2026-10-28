// GeolocationPosition, GeolocationCoordinates and GeolocationPositionError
// members: brand-checked getters over reserved slots and the [Default] toJSON
// (docs/owners/GEOLOCATION.md).
#include "web_geolocation/internal.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::web_geolocation {
bool PositionGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args.thisv(), &kPositionClass, slot ? "get timestamp" : "get coords", &self))
    return false;
  args.rval().set(JS::GetReservedSlot(self, slot));
  return JS_WrapValue(cx, args.rval());
}
bool CoordsGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  static const char* const names[] = {"get latitude", "get longitude", "get accuracy", "get altitude",
                                      "get altitudeAccuracy", "get heading", "get speed"};
  const int32_t slot = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args.thisv(), &kCoordsClass, names[slot], &self)) return false;
  args.rval().set(JS::GetReservedSlot(self, slot));
  return true;
}
bool ErrorGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args.thisv(), &kErrorClass, slot ? "get message" : "get code", &self))
    return false;
  args.rval().set(JS::GetReservedSlot(self, slot));
  return JS_WrapValue(cx, args.rval());
}
namespace {
// [Default] toJSON: a plain object of the attributes, in member order.
bool CoordsObject(JSContext* cx, JS::HandleObject coords, JS::MutableHandleValue out) {
  static const char* const names[] = {"latitude", "longitude", "altitude", "accuracy",
                                      "altitudeAccuracy", "heading", "speed"};
  static const Field fields[] = {Latitude, Longitude, Altitude, Accuracy, AltitudeAccuracy, Heading, Speed};
  JS::RootedObject json(cx, JS_NewPlainObject(cx));
  if (!json) return false;
  for (size_t i = 0; i < 7; ++i) {
    JS::RootedValue value(cx, JS::GetReservedSlot(coords, fields[i]));
    if (!JS_DefineProperty(cx, json, names[i], value, JSPROP_ENUMERATE)) return false;
  }
  out.setObject(*json);
  return true;
}
}
bool CoordsJson(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  return Receiver(cx, args.thisv(), &kCoordsClass, "toJSON", &self) &&
         CoordsObject(cx, self, args.rval());
}
bool PositionJson(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args.thisv(), &kPositionClass, "toJSON", &self)) return false;
  JS::RootedObject coords(cx, &JS::GetReservedSlot(self, PositionCoords).toObject());
  JS::RootedValue coordsJson(cx), timestamp(cx, JS::GetReservedSlot(self, PositionTimestamp));
  JS::RootedObject json(cx, JS_NewPlainObject(cx));
  if (!json || !CoordsObject(cx, coords, &coordsJson) ||
      !JS_DefineProperty(cx, json, "coords", coordsJson, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, json, "timestamp", timestamp, JSPROP_ENUMERATE)) return false;
  args.rval().setObject(*json);
  return true;
}
}  // namespace ruyijs::web_geolocation
