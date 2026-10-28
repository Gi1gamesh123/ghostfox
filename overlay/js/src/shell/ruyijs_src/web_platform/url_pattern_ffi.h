// C ABI of vendor/urlpattern-native (the Firefox urlpattern 0.6.0 crate behind a
// ruyijs glue) and the SpiderMonkey regexp callbacks it calls back into.
#ifndef RUYIJS_WEB_PLATFORM_URL_PATTERN_FFI_H
#define RUYIJS_WEB_PLATFORM_URL_PATTERN_FFI_H
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
struct JSContext;
class JSObject;
extern "C" {
struct RyStr { const uint8_t* ptr; size_t len; bool valid; };
// protocol, username, password, hostname, port, pathname, search, hash, baseURL
struct RyInit { RyStr fields[9]; };
void* ruyijs_urlpattern_parse_string(RyStr input, RyStr base, bool ignore_case);
void* ruyijs_urlpattern_parse_init(const RyInit* init, bool ignore_case);
void ruyijs_urlpattern_free(void* pattern);
bool ruyijs_urlpattern_has_regexp_groups(const void* pattern);
const void* ruyijs_urlpattern_component(const void* pattern, uint32_t index);  // 0 protocol .. 7 hash
RyStr ruyijs_urlpattern_component_pattern(const void* component);
bool ruyijs_urlpattern_component_regexp_empty(const void* component);
size_t ruyijs_urlpattern_component_group_count(const void* component);
RyStr ruyijs_urlpattern_component_group_name(const void* component, size_t index);
bool ruyijs_urlpattern_component_matches(const void* component, RyStr input, bool match_only, void* sink);
bool ruyijs_urlpattern_match_input_string(RyStr url, RyStr base, void* sink);
bool ruyijs_urlpattern_match_input_init(const RyInit* init, void* sink);
void ruyijs_urlpattern_push_capture(void* captures, RyStr value);
// Implemented in url_pattern_regexp.cpp.
bool ruyijs_urlpattern_regexp_valid(const uint16_t* pattern, size_t len, bool ignore_case);
bool ruyijs_urlpattern_regexp_exec(const uint16_t* pattern, size_t len, bool ignore_case, const uint8_t* text,
                                   size_t text_len, bool match_only, bool* matched, void* captures);
void ruyijs_urlpattern_sink(void* sink, RyStr value);
}
namespace ruyijs::web_platform {
using UrlPatternValues = std::vector<std::optional<std::string>>;  // the sink target
// The realm whose RegExp engine serves crate callbacks for the current call.
class UrlPatternRegExpScope {
 public:
  UrlPatternRegExpScope(JSContext* cx, JSObject* global);
  ~UrlPatternRegExpScope();
 private:
  JSContext* previousCx_;
  JSObject* previousGlobal_;
};
inline RyStr RyOf(const std::string& s) { return {reinterpret_cast<const uint8_t*>(s.data()), s.size(), true}; }
inline RyStr RyNone() { return {nullptr, 0, false}; }
inline std::string RyString(RyStr s) {
  return s.valid && s.len ? std::string(reinterpret_cast<const char*>(s.ptr), s.len) : std::string();
}
// Firefox record order for exec() groups: nsTHashMap<nsCStringHashKey> iteration
// (PLDHashTable slot order) after inserting `names` in group order.
std::vector<size_t> UrlPatternGroupOrder(const std::vector<std::string>& names);
}  // namespace ruyijs::web_platform
#endif
