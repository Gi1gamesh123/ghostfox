/* C stubs for the two Rust FFI components that the upstream wheel ships
 * prebuilt but the source repo does not (vendor/urlpattern-native and the
 * qcms color adapter). Every entry point degrades gracefully:
 *  - URLPattern construction/matching fails (constructor throws TypeError);
 *  - ICC profile conversion is skipped (pixels pass through unchanged). */
#include "web_platform/url_pattern_ffi.h"

using ruyijs::web_platform::RyNone;

extern "C" {

void* ruyijs_urlpattern_parse_string(RyStr, RyStr, bool) { return nullptr; }
void* ruyijs_urlpattern_parse_init(const RyInit*, bool) { return nullptr; }
void ruyijs_urlpattern_free(void*) {}
bool ruyijs_urlpattern_has_regexp_groups(const void*) { return false; }
const void* ruyijs_urlpattern_component(const void*, uint32_t) {
  return nullptr;
}
RyStr ruyijs_urlpattern_component_pattern(const void*) { return RyNone(); }
bool ruyijs_urlpattern_component_regexp_empty(const void*) { return true; }
size_t ruyijs_urlpattern_component_group_count(const void*) { return 0; }
RyStr ruyijs_urlpattern_component_group_name(const void*, size_t) {
  return RyNone();
}
bool ruyijs_urlpattern_component_matches(const void*, RyStr, bool, void*) {
  return false;
}
bool ruyijs_urlpattern_match_input_string(RyStr, RyStr, void*) {
  return false;
}
bool ruyijs_urlpattern_match_input_init(const RyInit*, void*) {
  return false;
}
void ruyijs_urlpattern_push_capture(void*, RyStr) {}
bool ruyijs_image_srgb_to_profile(uint8_t*, size_t, const uint8_t*, size_t) {
  return false;
}
}
