#include "browser_environment/state.h"
#include "browser_environment/window_clone.h"
#include "browser_environment/window_members.h"

namespace ruyijs::browser_environment {
// Window retains its public binding; serialization and transfer ownership use
// the MessageChannel owner's single engine path, bound by the coordinator.
bool CloneValue(State& state, JS::HandleValue source, JS::HandleValue transferable,
                JS::MutableHandleValue result, JS::HandleObject destination) {
  JS::RootedObject ports(state.cx);
  return CloneValue(state, source, transferable, result, destination, &ports);
}
bool CloneValue(State& state, JS::HandleValue source, JS::HandleValue transferable,
                JS::MutableHandleValue result, JS::HandleObject destination, JS::MutableHandleObject ports) {
  JSContext* cx = state.cx;
  if (!state.clonePort) return Error(cx, "Message clone owner is not installed");
  JS::RootedObject target(cx, destination ? destination.get() : state.page.get());
  return state.clonePort(state.cloneContext, source, transferable, target, result, ports);
}
}
