#ifndef RUYIJS_BROWSER_ENVIRONMENT_EXTERNAL_H
#define RUYIJS_BROWSER_ENVIRONMENT_EXTERNAL_H

namespace ruyijs::browser_environment {
struct State;

// The donor Window.external interface (dom/webidl/External.webidl). It is
// [LegacyNoInterfaceObject], so there is no global `External` constructor; this
// is the single installer of the prototype and of the one per-realm instance
// the Window facade exposes. Both operations are no-ops in the donor
// (dom/base/External.h), so they return undefined.
bool InstallExternalSurface(State& state);
}  // namespace ruyijs::browser_environment
#endif
