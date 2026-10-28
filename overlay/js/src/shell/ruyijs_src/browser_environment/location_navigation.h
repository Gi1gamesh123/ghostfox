#ifndef RUYIJS_BROWSER_ENVIRONMENT_LOCATION_NAVIGATION_H
#define RUYIJS_BROWSER_ENVIRONMENT_LOCATION_NAVIGATION_H
#include "js/CallArgs.h"
#include "js/TypeDecls.h"

namespace ruyijs::browser_environment {
struct State;

// Slot ids for the Location navigation family. assign/replace/reload are
// methods of the unforgeable Location interface, so they are installed on the
// instance next to the accessors rather than on Location.prototype.
enum LocationMember : int32_t {
  kLocationAssign = 600, kLocationReplace, kLocationReload
};

// The one setter behind href and every writeable URL component. The accessor's
// slot says which component changed: 0 href, 1 protocol, 2 host, 3 hostname,
// 4 port, 5 pathname, 6 search, 7 hash. `origin` (8) stays read-only.
bool LocationSetter(JSContext* cx, unsigned argc, JS::Value* vp);

// assign/replace/reload dispatch.
bool LocationMethod(JSContext* cx, unsigned argc, JS::Value* vp);

// Installs the three navigation methods on the Location instance.
bool InstallLocationMembers(State& state, JS::HandleObject location);

// Delivers one trusted hashchange through the Interaction owner's port.
bool DeliverHashChange(State& state, const char* oldURL, const char* newURL);
}  // namespace ruyijs::browser_environment
#endif
