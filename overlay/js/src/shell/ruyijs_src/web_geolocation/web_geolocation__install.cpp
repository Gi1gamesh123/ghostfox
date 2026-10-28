// The one production installer of the Geolocation owner, its Browser
// Environment "geolocation" group port and its [SecureContext] exposure.
#include "web_geolocation/internal.h"
#include "runtime/page_helpers.h"
#include "runtime/page_reference.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/PropertyDescriptor.h"
#include "js/Realm.h"

namespace ruyijs::web_geolocation {
namespace {
const char* const kFieldNames[FieldCount] = {"latitude", "longitude", "accuracy", "altitude",
    "altitudeAccuracy", "heading", "speed", "timestamp"};
bool Navigator(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  args.rval().setObject(*state.instance);
  return JS_WrapValue(cx, args.rval());
}
// WebIDL puts `constructor` after the members; Define created it first.
bool Last(JSContext* cx, WebFace& face) {
  JS::RootedValue constructor(cx, JS::ObjectValue(*face.constructor));
  return JS_DeleteProperty(cx, face.prototype, "constructor") &&
         JS_DefineProperty(cx, face.prototype, "constructor", constructor, 0);
}
bool Constants(JSContext* cx, JS::HandleObject target) {
  const unsigned flags = JSPROP_READONLY | JSPROP_ENUMERATE | JSPROP_PERMANENT;
  return JS_DefineProperty(cx, target, "PERMISSION_DENIED", 1, flags) &&
         JS_DefineProperty(cx, target, "POSITION_UNAVAILABLE", 2, flags) &&
         JS_DefineProperty(cx, target, "TIMEOUT", 3, flags);
}
// Firefox's Navigator lists `geolocation` right after `gpu`: move every later
// member behind it, keeping each descriptor (and so its function identity).
bool AfterGpu(JSContext* cx, JS::HandleObject navigator) {
  JS::RootedIdVector keys(cx);
  if (!js::GetPropertyKeys(cx, navigator, JSITER_OWNONLY | JSITER_HIDDEN, &keys)) return false;
  bool after = false;
  JS::RootedId id(cx);
  JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> desc(cx);
  for (size_t i = 0; i < keys.length(); ++i) {
    id = keys[i];
    if (!id.isString()) continue;
    bool gpu = false, geo = false;
    if (!JS_StringEqualsAscii(cx, id.toString(), "gpu", &gpu) ||
        !JS_StringEqualsAscii(cx, id.toString(), "geolocation", &geo)) return false;
    if (gpu) { after = true; continue; }
    if (!after || geo) continue;
    if (!JS_GetOwnPropertyDescriptorById(cx, navigator, id, &desc) || desc.isNothing())
      return false;
    if (!desc->configurable()) continue;
    JS::Rooted<JS::PropertyDescriptor> copy(cx, *desc);
    JS::ObjectOpResult removed;
    if (!JS_DeletePropertyById(cx, navigator, id, removed) || !removed.ok() ||
        !JS_DefinePropertyById(cx, navigator, id, copy)) return false;
  }
  return true;
}
}
Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, Realm* origin)
    : state_(std::make_unique<State>(cx, page, helpers)) {
  state_->config = origin ? origin->state_->config : std::make_shared<Config>();
}
Realm::~Realm() = default;
void Realm::SetPermissionPort(PermissionPort port, void* context) {
  state_->permissionPort = port;
  state_->permissionContext = context;
}
void Realm::SetTaskPort(TaskPort port, void* context) {
  state_->taskPort = port;
  state_->taskContext = context;
}
bool Realm::Install(JS::HandleObject navigatorPrototype) {
  State& s = *state_;
  JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.kit.page);
  JS::RootedValue date(cx), now(cx);
  if (!navigatorPrototype || !JS_GetProperty(cx, s.kit.page, "Date", &date) || !date.isObject())
    return false;
  s.date = &date.toObject();
  if (!JS_GetProperty(cx, s.date, "now", &now) || !now.isObject()) return false;
  s.dateNow = &now.toObject();
  const char* G = "Geolocation";
  const char* P = "GeolocationPosition";
  const char* C = "GeolocationCoordinates";
  const char* E = "GeolocationPositionError";
  if (!s.kit.Define(G, &kGeolocationClass, nullptr, WebKit::Reject, &s.geolocation) ||
      !s.kit.Method(s.geolocation, G, "getCurrentPosition", GetCurrentPosition, 1, 0) ||
      !s.kit.Method(s.geolocation, G, "watchPosition", GetCurrentPosition, 1, 1) ||
      !s.kit.Method(s.geolocation, G, "clearWatch", ClearWatch, 1, 0) ||
      !Last(cx, s.geolocation) ||
      !s.kit.Define(P, &kPositionClass, nullptr, WebKit::Reject, &s.position) ||
      !s.kit.Method(s.position, P, "toJSON", PositionJson, 0, 0) ||
      !s.kit.Accessor(s.position, P, "coords", PositionGet, nullptr, PositionCoords) ||
      !s.kit.Accessor(s.position, P, "timestamp", PositionGet, nullptr, PositionTimestamp) ||
      !Last(cx, s.position) ||
      !s.kit.Define(C, &kCoordsClass, nullptr, WebKit::Reject, &s.coords) ||
      !s.kit.Method(s.coords, C, "toJSON", CoordsJson, 0, 0)) return false;
  // Member order is Firefox's; each getter reads the slot of its Field.
  const Field order[] = {Latitude, Longitude, Altitude, Accuracy, AltitudeAccuracy, Heading, Speed};
  for (Field field : order)
    if (!s.kit.Accessor(s.coords, C, kFieldNames[field], CoordsGet, nullptr, field)) return false;
  if (!Last(cx, s.coords) ||
      !s.kit.Define(E, &kErrorClass, nullptr, WebKit::Reject, &s.error) ||
      !s.kit.Accessor(s.error, E, "code", ErrorGet, nullptr, ErrorCode) ||
      !s.kit.Accessor(s.error, E, "message", ErrorGet, nullptr, ErrorMessage) ||
      !Constants(cx, s.error.prototype) || !Constants(cx, s.error.constructor) ||
      !Last(cx, s.error)) return false;
  s.instance = s.kit.New(&kGeolocationClass, s.geolocation.prototype);
  if (!s.instance) return false;
  JS::SetReservedSlot(s.instance, GeoState, JS::PrivateValue(&s));
  JS::SetReservedSlot(s.instance, GeoPosition, JS::ObjectValue(*s.position.constructor));
  JS::SetReservedSlot(s.instance, GeoCoordinates, JS::ObjectValue(*s.coords.constructor));
  JS::RootedValue reference(cx, JS::ObjectValue(*s.instance));
  JS::RootedObject getter(cx, s.kit.Function(Navigator, "get geolocation", 0, 0)), setter(cx);
  if (!getter || !WritePageReference(s.kit.page, PageReference::Geolocation, reference)) return false;
  return JS_DefineProperty(cx, navigatorPrototype, "geolocation", getter, setter,
                           JSPROP_ENUMERATE) &&
         AfterGpu(cx, navigatorPrototype) &&
         s.kit.helpers->Accessor("Navigator", navigatorPrototype, "geolocation") &&
         s.kit.helpers->Object(G, s.instance) && ConfigureExposure(cx, s.kit.page);
}
bool Realm::Configure(JS::HandleObject group) {
  State& s = *state_;
  JSContext* cx = s.cx;
  for (int field = 0; field < FieldCount; ++field) {
    bool found = false;
    if (!JS_HasProperty(cx, group, kFieldNames[field], &found)) return false;
    if (!found) continue;
    JS::RootedValue value(cx);
    if (!JS_GetProperty(cx, group, kFieldNames[field], &value)) return false;
    s.config->set[field] = true;
    s.config->null[field] = value.isNull();
    s.config->values[field] = value.isNumber() ? value.toNumber() : 0;
  }
  return true;
}
bool ConfigureExposure(JSContext* cx, JS::HandleObject page) {
  JSAutoRealm realm(cx, page);
  JS::RootedValue instance(cx), secure(cx);
  if (!ReadPageReference(page, PageReference::Geolocation, &instance)) return false;
  if (!instance.isObject()) return true;  // Not installed in this realm.
  JS::RootedObject geolocation(cx, &instance.toObject());
  JS::RootedValue position(cx, JS::GetReservedSlot(geolocation, GeoPosition));
  JS::RootedValue coords(cx, JS::GetReservedSlot(geolocation, GeoCoordinates));
  if (!JS_GetProperty(cx, page, "isSecureContext", &secure)) return false;
  if (!secure.isTrue())
    return JS_DeleteProperty(cx, page, "GeolocationPosition") &&
           JS_DeleteProperty(cx, page, "GeolocationCoordinates");
  return JS_DefineProperty(cx, page, "GeolocationPosition", position, 0) &&
         JS_DefineProperty(cx, page, "GeolocationCoordinates", coords, 0);
}
}  // namespace ruyijs::web_geolocation
