// The window's one scroll operation and its notification (docs/owners/
// BROWSER_ENVIRONMENT.md, 2026-10-22). Firefox 158 (artifacts/scroll-event):
// a scroll position is clamped to [0, scrollMax], the scrollMax the Geometry
// layout reports through the scrollExtent port; once the position moved, the
// next refresh tick fires one trusted `scroll` then one `scrollend` UIEvent at
// the document (it bubbles to the window), however many scroll calls the task
// made. Here that tick is the next task on the context's queue: the events
// still come after the caller's microtasks and before animation frames.
#include "browser_environment/state.h"
#include "browser_environment/window_members.h"
#include "jsfriendapi.h"
#include <algorithm>
#include <cmath>

namespace ruyijs::browser_environment {
namespace {
// The largest scroll position on each axis; without layout (no port) the
// position is only kept non-negative.
bool ScrollLimit(State& state, double* maxX, double* maxY) {
  *maxX = *maxY = INFINITY;
  if (!state.scrollExtent) return true;
  double width = 0, height = 0, right = 0, bottom = 0;
  if (!state.ReadViewport(&width, &height) ||
      !state.scrollExtent(state.scrollExtentContext, &right, &bottom)) return false;
  // The extent is in viewport coordinates, so it moves with the current scroll.
  const double x = right + state.scrollX - width, y = bottom + state.scrollY - height;
  *maxX = std::max(0.0, std::round(std::isfinite(x) ? x : 0));
  *maxY = std::max(0.0, std::round(std::isfinite(y) ? y : 0));
  return true;
}
bool NotifyScroll(JSContext*, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  args.rval().setUndefined();
  state.scrollPending = false;
  if (!state.scrollEvent) return true;  // The document retired before the task ran.
  return state.scrollEvent(state.scrollEventContext, "scroll") &&
         state.scrollEvent(state.scrollEventContext, "scrollend");
}
}  // namespace

bool ScrollWindowTo(State& state, double x, double y) {
  double maxX = 0, maxY = 0;
  if (!ScrollLimit(state, &maxX, &maxY)) return false;
  x = std::clamp(x, 0.0, maxX);
  y = std::clamp(y, 0.0, maxY);
  if (x == state.scrollX && y == state.scrollY) return true;
  state.scrollX = x;
  state.scrollY = y;
  if (state.scrollPending || !state.postTask || !state.scrollEvent) return true;
  JSContext* cx = state.cx;
  JSFunction* task = js::NewFunctionWithReserved(cx, NotifyScroll, 0, 0, "scroll");
  if (!task) return false;
  JS::RootedObject object(cx, JS_GetFunctionObject(task));
  js::SetFunctionNativeReserved(object, 0, JS::PrivateValue(&state));
  state.scrollPending = true;
  return state.postTask(state.postTaskContext, object);
}
}  // namespace ruyijs::browser_environment
