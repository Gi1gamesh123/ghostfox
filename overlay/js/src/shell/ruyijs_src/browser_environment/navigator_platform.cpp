#include "browser_environment/state.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "js/Wrapper.h"
#include "jsfriendapi.h"
#include "runtime/runtime.h"

namespace ruyijs::browser_environment {
namespace {
State* Owner(JSContext* cx, const JS::CallArgs& args) {
  JS::RootedObject self(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!self || JS::GetClass(self) != ClassFor(Kind::Navigator)) {
    ReceiverError(cx, args, "Navigator");
    return nullptr;
  }
  auto slot = JS::GetReservedSlot(self, 0);
  if (slot.isUndefined() || !slot.toPrivate()) {
    ReceiverError(cx, args, "Navigator");
    return nullptr;
  }
  return static_cast<State*>(slot.toPrivate());
}
bool GetGamepads(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = Owner(cx, args);
  if (!state) return false;
  state->Observe("Navigator", "getGamepads", int(ApiAccess::Call));
  JS::RootedObject result(cx, JS::NewArrayObject(cx, 0));
  if (!result) return false;
  args.rval().setObject(*result);
  return true;
}
bool SendBeacon(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = Owner(cx, args);
  if (!state) return false;
  state->Observe("Navigator", "sendBeacon", int(ApiAccess::Call));
  if (!argc) return Error(cx, "Navigator.sendBeacon requires a URL");
  JS::RootedString input(cx, JS::ToString(cx, args.get(0)));
  std::string url;
  if (!input || !ToUTF8(cx, input, &url)) return false;
  JS::RootedValue body(cx, argc > 1 ? args[1] : JS::UndefinedHandleValue);
  bool accepted = false;
  if (state->sendBeacon &&
      !state->sendBeacon(state->sendBeaconContext, cx, url, body, &accepted)) return false;
  args.rval().setBoolean(accepted);
  return true;
}
}  // namespace

bool State::NavigatorPlatformMembers() {
  JS::RootedObject prototype(cx, navigatorPrototype);
  JS::RootedObject gamepads(cx, Function(GetGamepads, "getGamepads", 0, 0));
  JS::RootedObject beacon(cx, Function(SendBeacon, "sendBeacon", 1, 0));
  if (!prototype || !gamepads || !beacon ||
      !JS_DefineProperty(cx, prototype, "getGamepads", gamepads, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, prototype, "sendBeacon", beacon, JSPROP_ENUMERATE)) return false;
  return helpers->Member(gamepads, "Navigator", "getGamepads", "call") &&
         helpers->Member(beacon, "Navigator", "sendBeacon", "call");
}
}  // namespace ruyijs::browser_environment
