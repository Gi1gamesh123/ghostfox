#ifndef RUYIJS_WEB_GPU_REALM_H
#define RUYIJS_WEB_GPU_REALM_H
#include <memory>
#include "js/RootingAPI.h"
struct JSContext;
namespace ruyijs { class PageHelpers; }
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::browser_environment { class Environment; }
namespace ruyijs::web_gpu {
struct State;
// The WebGPU owner: the GPU* object graph over the imported
// domain/webgpu capability aggregate. navigator.gpu stays with the
// browser_environment Navigator owner; this realm appends its single "gpu"
// accessor to that realm's Navigator prototype and keeps the GPU instance in
// its own state, so there is exactly one owner of the accessor and one of the
// Navigator prototype.
class Realm final {
 public:
  Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
        interaction::Realm& events, browser_environment::Environment& environment);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  bool Install();
  // Removes or restores the [SecureContext] WebGPU surface for the realm's
  // current secure-context state (runtime/secure_exposure.cpp).
  bool ConfigureExposure();
  // The WebGPU state the environment port binds to. The realm owns it; the
  // port's void* context is this State*, never the Realm wrapper.
  State* state() const noexcept;
 private:
  std::unique_ptr<State> state_;
};
}
#endif
