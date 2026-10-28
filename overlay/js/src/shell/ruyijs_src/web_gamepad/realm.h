#ifndef RUYIJS_WEB_GAMEPAD_REALM_H
#define RUYIJS_WEB_GAMEPAD_REALM_H
#include <memory>
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs { class PageHelpers; }
namespace ruyijs::web_gamepad {
struct State;
// Gamepad device interfaces for one Window realm (docs/owners/GAMEPAD.md):
// Gamepad, GamepadButton, GamepadHapticActuator and GamepadPose. This runtime
// has no gamepad device, so navigator.getGamepads() stays empty and no
// instance is created; the interfaces and their receiver checks follow Firefox.
class Realm final {
 public:
  Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  bool Install();
 private:
  std::unique_ptr<State> state_;
};
}
#endif
