#ifndef RUYIJS_BROWSER_ENVIRONMENT_BROWSER_CHROME_H
#define RUYIJS_BROWSER_ENVIRONMENT_BROWSER_CHROME_H
#include "browser_environment/state.h"
namespace ruyijs::browser_environment {
// The existing Window prototype owns the realm's six stable bar references.
inline constexpr unsigned kWindowHandlerStoreSlot = 7;
inline constexpr unsigned kWindowFacadeSlots = 8;
inline constexpr unsigned kNavigatorActivationSlot = 1;
inline constexpr const char* kBarNames[] = {
  "locationbar", "menubar", "personalbar", "scrollbars", "statusbar", "toolbar"};
extern const JSClass kWindowFacadeClass, kBarPropClass, kUserActivationClass;
bool InstallBrowserChrome(State& state);
State* WindowOwner(JSContext*, const JS::CallArgs&, JS::MutableHandleObject prototype);
State* WindowGlobalOwner(JSContext*, JS::HandleObject, JS::MutableHandleObject prototype);
bool ChromeValueGet(JSContext*, unsigned, JS::Value*);
bool ChromeVisibleSet(JSContext*, unsigned, JS::Value*);
bool ChromeBarGet(JSContext*, unsigned, JS::Value*);
bool ChromeBarSet(JSContext*, unsigned, JS::Value*);
bool ChromeActivationGet(JSContext*, unsigned, JS::Value*);
}
#endif
