// navigator.gpu: the one accessor this domain appends to the Navigator
// prototype owned by browser_environment. The GPU instance is realm-local and
// created once, so repeat reads keep a stable identity.
#include "web_gpu/internal.h"

#include "js/Realm.h"

namespace ruyijs::web_gpu {

bool NavigatorGpu(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = static_cast<State*>(WebKit::Owner(args));
  if (!state) return WebKit::Fail(cx, "Navigator member called on an incompatible receiver");
  if (!state->navigatorGpu) {
    const JSAutoRealm realm(cx, state->kit.page);
    JS::RootedObject gpu(cx, state->kit.New(&kGpuClass, state->gpu.prototype));
    if (!gpu) return false;
    JS::SetReservedSlot(gpu, 0, JS::NullValue());
    state->navigatorGpu = gpu;
  }
  args.rval().setObject(*state->navigatorGpu);
  return true;
}

}  // namespace ruyijs::web_gpu
