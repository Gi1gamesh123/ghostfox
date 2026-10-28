#ifndef RUYIJS_DOMAIN_BROWSER_ENVIRONMENT_TOUCH_EXPOSURE_H
#define RUYIJS_DOMAIN_BROWSER_ENVIRONMENT_TOUCH_EXPOSURE_H

namespace ruyijs::domain::browser_environment {
enum class TouchEventsOverride { Auto, Enabled, Disabled };

// Interface exposure and Navigator's reported point count are separate inputs.
// Native platform probing belongs to the adapter, not this value rule.
constexpr bool ResolveTouchEvents(TouchEventsOverride policy,
                                  bool platform_supported) noexcept {
  if (policy == TouchEventsOverride::Enabled) return true;
  if (policy == TouchEventsOverride::Disabled) return false;
  return platform_supported;
}
}
#endif
