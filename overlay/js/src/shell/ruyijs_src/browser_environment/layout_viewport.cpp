#include "browser_environment/state.h"
#include "browser_environment/window_members.h"

namespace ruyijs::browser_environment {
bool Environment::ReadLayoutViewport(double* width, double* height, double* x, double* y) {
  if (!state_->ReadViewport(width, height)) return false;
  *x = state_->scrollX; *y = state_->scrollY;
  return true;
}
bool Environment::ScrollViewportTo(double x, double y) {
  return ScrollWindowTo(*state_, x, y);
}
bool State::ReadViewport(double* width, double* height) {
  if (viewportPort) return viewportPort(viewportPortContext, width, height);
  if (shared) { *width = 0; *height = 0; return true; }
  JS::RootedValue w(cx), h(cx);
  if (!Read(Kind::Viewport, 0, ValueType::Integer, &w) ||
      !Read(Kind::Viewport, 1, ValueType::Integer, &h)) return false;
  *width = w.toNumber(); *height = h.toNumber();
  return true;
}
}
