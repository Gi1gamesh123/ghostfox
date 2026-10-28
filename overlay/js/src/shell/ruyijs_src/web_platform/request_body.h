#ifndef RUYIJS_WEB_PLATFORM_REQUEST_BODY_H
#define RUYIJS_WEB_PLATFORM_REQUEST_BODY_H
#include <string>
#include "js/RootingAPI.h"

namespace ruyijs::web_platform {
// Internal read port. Owns a bounded snapshot, never a second platform object.
struct RequestBody {
  std::string bytes, contentType;
  bool tooLarge = false;
};
inline constexpr size_t kRequestBodyLimit = 1024 * 1024;
bool ReadRequestBody(JSContext*, JS::HandleValue, RequestBody*);
bool ReadFormRequestBody(JSContext*, JS::HandleObject, RequestBody*);
bool ReadRequestText(JSContext*, JS::HandleValue, std::string*);
std::string NormalizeFormLines(const std::string&);
}
#endif
