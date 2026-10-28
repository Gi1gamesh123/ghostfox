// Browser Environment interfaces without instances in this runtime (Firefox 158,
// Exposed=Window): NavigationPrecommitController, handed to a NavigateEvent's
// precommit handlers (no precommit phase runs here), and PictureInPictureWindow,
// returned by HTMLVideoElement.requestPictureInPicture (no window ever opens).
// The interface objects keep Firefox's shape and every member fails its brand
// check with Firefox's text (D149).
#include "browser_environment/navigation_api.h"
#include "browser_environment/window_legacy.h"
#include "interaction/realm.h"
#include "runtime/page_helpers.h"
#include "js/Object.h"

namespace ruyijs::browser_environment {
namespace {
const JSClass kPrecommitClass = {"NavigationPrecommitController", 0};
const JSClass kPipWindowClass = {"PictureInPictureWindow", 0};
// Slot 0: NavigationPrecommitController, 1: PictureInPictureWindow.
bool Brand(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  return WebKit::FailBrand(cx, args, WebKit::Slot(args) ? "PictureInPictureWindow" : "NavigationPrecommitController");
}
}  // namespace

bool InstallNavigationPrecommit(NavigationState& n) {
  JS::RootedObject object(n.env.cx, JS::GetRealmObjectPrototype(n.env.cx));
  WebFace face(n.env.cx);
  return n.kit.Define("NavigationPrecommitController", &kPrecommitClass, object, WebKit::Reject, &face) &&
         n.kit.Method(face, "NavigationPrecommitController", "redirect", Brand, 1, 0) &&
         n.kit.Method(face, "NavigationPrecommitController", "addHandler", Brand, 1, 0);
}

bool InstallPictureInPictureWindow(WindowLegacyState& w) {
  JS::RootedObject target(w.env.cx, w.events.prototype());
  WebFace face(w.env.cx);
  return w.kit.Define("PictureInPictureWindow", &kPipWindowClass, target, WebKit::Reject, &face) &&
         w.kit.Accessor(face, "PictureInPictureWindow", "width", Brand, nullptr, 1) &&
         w.kit.Accessor(face, "PictureInPictureWindow", "height", Brand, nullptr, 1) &&
         w.kit.Accessor(face, "PictureInPictureWindow", "onresize", Brand, Brand, 1);
}
}  // namespace ruyijs::browser_environment
