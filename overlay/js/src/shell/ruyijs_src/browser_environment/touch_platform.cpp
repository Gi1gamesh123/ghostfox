#include "browser_environment/touch_platform.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ruyijs::browser_environment {
bool PlatformSupportsTouchEvents() noexcept {
#ifdef _WIN32
  const int capabilities = ::GetSystemMetrics(SM_DIGITIZER);
  // WinUtils::IsTouchDeviceSupportPresent includes pen devices when the donor's
  // dom.w3c_pointer_events.scroll_by_pen.enabled preference is true (the default).
  // This adapter implements that default; it does not emulate a preference store.
  constexpr int supported = NID_EXTERNAL_TOUCH | NID_INTEGRATED_TOUCH |
                            NID_EXTERNAL_PEN | NID_INTEGRATED_PEN;
  return (capabilities & NID_READY) != 0 && (capabilities & supported) != 0;
#else
  // Other platforms need their own native input adapter before auto is supported.
  return false;
#endif
}
}
