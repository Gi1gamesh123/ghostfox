#ifndef RUYIJS_WEB_SECURITY_POLICY_NAMES_H
#define RUYIJS_WEB_SECURITY_POLICY_NAMES_H
#include <string_view>

namespace ruyijs::web_security {
// Known names from donor dom/security/nsCSPUtils.h. Recognition for policy
// serialization is separate from the script-only enforcement of this slice.
inline bool KnownDirective(std::string_view name) {
  constexpr std::string_view names[] = {
      "default-src", "script-src", "object-src", "style-src", "img-src", "media-src",
      "frame-src", "font-src", "connect-src", "report-uri", "frame-ancestors",
      "reflected-xss", "base-uri", "form-action", "manifest-src", "upgrade-insecure-requests",
      "child-src", "block-all-mixed-content", "sandbox", "worker-src", "script-src-elem",
      "script-src-attr", "style-src-elem", "style-src-attr", "require-trusted-types-for",
      "trusted-types", "report-to"};
  for (const auto candidate : names) if (name == candidate) return true;
  return false;
}
}
#endif
