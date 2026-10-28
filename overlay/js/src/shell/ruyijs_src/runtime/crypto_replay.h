#ifndef RUYIJS_CRYPTO_REPLAY_H
#define RUYIJS_CRYPTO_REPLAY_H
#include "js/Value.h"
#include "js/RootingAPI.h"
#include <memory>
#include <string>
#include <vector>

namespace ruyijs {
class CryptoReplayPlan {
 public:
  bool Configure(JSContext*, JS::HandleObject request, bool frozen, JS::MutableHandleValue result);
  bool Bind(JSContext*, JS::HandleObject page, const std::string& url);
  bool Status(JSContext*, JS::MutableHandleValue result);
 private:
  struct Entry {
    Entry(JSContext* cx, std::string name, JS::HandleObject input) : url(std::move(name)), tape(cx,input) {}
    std::string url;
    JS::PersistentRootedObject tape;
    bool bound = false;
  };
  std::vector<std::unique_ptr<Entry>> entries_;
};
}
#endif
