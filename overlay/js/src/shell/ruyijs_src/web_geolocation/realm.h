#ifndef RUYIJS_WEB_GEOLOCATION_REALM_H
#define RUYIJS_WEB_GEOLOCATION_REALM_H
#include <memory>
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs { class PageHelpers; }
namespace ruyijs::web_geolocation {
struct State;
// The Geolocation owner of one Window realm (docs/owners/GEOLOCATION.md). The
// main context owns the one position Config; iframe and successor realms share
// it through `origin`, so the Context has a single position source. Permission
// is never stored here: it is read from the Permissions owner's table.
class Realm final {
 public:
  Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, Realm* origin);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  // Installs the four interfaces and appends the one `geolocation` accessor to
  // the Navigator prototype browser_environment owns.
  bool Install(JS::HandleObject navigatorPrototype);
  // Browser Environment "geolocation" group port, already validated.
  bool Configure(JS::HandleObject group);
  // Permissions owner port: state of PermissionName "geolocation"
  // (0 granted, 1 denied, 2 prompt).
  using PermissionPort = int (*)(void* context);
  void SetPermissionPort(PermissionPort port, void* context);
  // Scheduling port: queues one callback task on the Context's macrotask queue.
  using TaskPort = bool (*)(void* context, JS::HandleObject task);
  void SetTaskPort(TaskPort port, void* context);
 private:
  std::unique_ptr<State> state_;
};
// GeolocationPosition and GeolocationCoordinates are [SecureContext]; applied
// at the existing exposure points next to web_crypto::ConfigureExposure.
bool ConfigureExposure(JSContext* cx, JS::HandleObject page);
}
#endif
