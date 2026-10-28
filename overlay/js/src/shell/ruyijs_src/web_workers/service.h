#ifndef RUYIJS_WEB_WORKERS_SERVICE_H
#define RUYIJS_WEB_WORKERS_SERVICE_H
#include <memory>
#include <string>
#include <string_view>

namespace ruyijs::web_workers {
inline constexpr char kServiceRequest = '\x05', kServiceReply = '\x06';
// Runtime binds an existing owner's session. Transport never interprets its data.
struct Service {
  std::shared_ptr<void> owner;
  bool (*request)(void*, std::string_view, std::string*) = nullptr;
};
using ServiceFactory = bool (*)(void*, Service*);
}
#endif
