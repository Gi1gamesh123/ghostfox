#ifndef RUYIJS_WEB_PLATFORM_OBJECT_URL_SESSION_H
#define RUYIJS_WEB_PLATFORM_OBJECT_URL_SESSION_H
#include <memory>
#include <string>
#include <string_view>

namespace ruyijs::web_platform {
struct State;
using ObjectUrlExchange = bool (*)(void*, std::string_view, std::string*);
// A lease into the original catalog, with transport IDs but no second byte store.
class ObjectUrlSession final {
 public:
  ObjectUrlSession(State&, std::string origin);
  ~ObjectUrlSession();
  bool Request(std::string_view, std::string*);
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}
#endif
