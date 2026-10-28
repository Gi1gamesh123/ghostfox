#ifndef RUYIJS_BROWSER_ENVIRONMENT_SCREEN_ORIENTATION_H
#define RUYIJS_BROWSER_ENVIRONMENT_SCREEN_ORIENTATION_H
#include "js/RootingAPI.h"
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::browser_environment {
struct State;
// The Screen orientation surface, split by responsibility so each file stays
// inside the 200-line gate:
//  * screen_orientation.cpp owns the ScreenOrientation interface and the one
//    realm-local instance behind [SameObject] Screen.prototype.orientation;
//  * screen_members.cpp owns the deprecated Screen.mozOrientation /
//    onmozorientationchange / mozLockOrientation / mozUnlockOrientation members
//    and the orientation accessor itself.
// Type and angle come from the existing screen profile; lock() takes the
// donor's non-fullscreen path. Exactly one production installer for every
// member listed here (docs/owners/BROWSER_ENVIRONMENT.md).
bool InstallScreenOrientation(State& state, interaction::Realm& events);
bool InstallScreenMembers(State& state);
// One orientation type string for ScreenOrientation.type and screen.mozOrientation,
// derived from the same width/height the Screen accessors report (donor
// ViewportSizeToOrientationType). Also used by screen_members.cpp.
bool OrientationTypeText(State& state, JS::MutableHandleValue result);
}  // namespace ruyijs::browser_environment
#endif
