#ifndef RUYIJS_WEB_GEOLOCATION_INTERNAL_H
#define RUYIJS_WEB_GEOLOCATION_INTERNAL_H
#include "web_geolocation/realm.h"
#include "runtime/web_kit.h"
#include <cstdint>
#include <memory>
#include <set>
#include <string>

namespace ruyijs::web_geolocation {
// The Context's one fixed position, in the profile group's field order:
// latitude, longitude, accuracy, altitude, altitudeAccuracy, heading, speed,
// timestamp. A field is either unset, null, or a finite number.
enum Field { Latitude, Longitude, Accuracy, Altitude, AltitudeAccuracy, Heading, Speed,
             Timestamp, FieldCount };
struct Config {
  double values[FieldCount] = {};
  bool set[FieldCount] = {};
  bool null[FieldCount] = {};
  bool HasPosition() const { return set[Latitude] && set[Longitude]; }
};
// Reserved slots. The instance keeps its State so a cross-realm receiver uses
// its own owner; the two [SecureContext] constructors live on the instance for
// ConfigureExposure.
enum GeoSlot { GeoState = 0, GeoPosition = 1, GeoCoordinates = 2, GeoSlotCount = 3 };
enum PositionSlot { PositionCoords = 0, PositionTimestamp = 1, PositionSlotCount = 2 };
enum ErrorSlot { ErrorCode = 0, ErrorMessage = 1, ErrorSlotCount = 2 };
// Coordinates keep the seven position fields at their Field index.
inline constexpr unsigned kCoordsSlots = Timestamp;
extern const JSClass kGeolocationClass, kPositionClass, kCoordsClass, kErrorClass;
// What a request resolves to, decided when it is made.
enum class Outcome : int32_t { Success = 0, Denied = 1, Unavailable = 2, Pending = 3 };
struct State {
  State(JSContext* cx, JS::HandleObject page, PageHelpers& helpers)
      : cx(cx), kit(cx, page, helpers, this), geolocation(cx), position(cx), coords(cx),
        error(cx), instance(cx), dateNow(cx), date(cx) {}
  JSContext* cx;
  WebKit kit;
  WebFace geolocation, position, coords, error;
  JS::PersistentRootedObject instance;
  // The realm's original Date.now: timestamps follow the Context clock and a
  // page reassigning Date cannot change them.
  JS::PersistentRootedObject dateNow, date;
  std::shared_ptr<Config> config;
  int32_t nextWatch = 1;
  std::set<int32_t> cleared;
  Realm::PermissionPort permissionPort = nullptr;
  void* permissionContext = nullptr;
  Realm::TaskPort taskPort = nullptr;
  void* taskContext = nullptr;
};
bool Receiver(JSContext* cx, JS::HandleValue self, const JSClass* clasp, const std::string& member,
              JS::MutableHandleObject out);
bool GetCurrentPosition(JSContext* cx, unsigned argc, JS::Value* vp);
bool ClearWatch(JSContext* cx, unsigned argc, JS::Value* vp);
bool PositionGet(JSContext* cx, unsigned argc, JS::Value* vp);
bool PositionJson(JSContext* cx, unsigned argc, JS::Value* vp);
bool CoordsGet(JSContext* cx, unsigned argc, JS::Value* vp);
bool CoordsJson(JSContext* cx, unsigned argc, JS::Value* vp);
bool ErrorGet(JSContext* cx, unsigned argc, JS::Value* vp);
}
#endif
