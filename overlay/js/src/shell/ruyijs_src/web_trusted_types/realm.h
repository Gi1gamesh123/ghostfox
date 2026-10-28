#ifndef RUYIJS_WEB_TRUSTED_TYPES_REALM_H
#define RUYIJS_WEB_TRUSTED_TYPES_REALM_H
#include <memory>
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs { class PageHelpers; }
namespace ruyijs::web_trusted_types {
struct State;
// The Trusted Types owner of one Window or Worker realm (docs/owners/TRUSTED_TYPES.md).
// It lives as long as the browsing context or worker service that installed it.
class Realm final {
 public:
  Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  // Installs the five interfaces and `trustedTypes`: on the window itself, or on
  // `globalPrototype` (WorkerGlobalScope.prototype) for a worker. Also installs
  // the process-wide eval hook once.
  bool Install(JS::HandleObject globalPrototype = nullptr);
 private:
  std::unique_ptr<State> state_;
};
// HostGetCodeForEval: a TrustedScript from any realm yields its string; every
// other object yields no code, so eval returns it unchanged.
bool CodeForEval(JSContext* cx, JS::HandleObject code, JS::MutableHandle<JSString*> out);
}
#endif
