/* ruyi-clone stub: Sanitizer default tables (empty = permissive defaults). */
#ifndef RUYIJS_FIREFOX_SANITIZER_DATA_H
#define RUYIJS_FIREFOX_SANITIZER_DATA_H
namespace ruyijs {
struct SanitizerElementData {
  const char* name;
  const char* ns;
  const char* const* attributes;
};
inline const SanitizerElementData kSanitizerDefaultElements[] = {};
inline const char* const kSanitizerDefaultAttributes[] = {nullptr};
inline const char* const kSanitizerEventAttributes[] = {"onerror", "onload"};
}  // namespace ruyijs
#endif
