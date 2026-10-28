#pragma once
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs::interaction {
// The AbortSignal capability lent by its owner (ResourceNetwork, abort.h):
// brand check, aborted flag and abort-algorithm registration. Interaction
// keeps no copied abort state; addEventListener's signal option and the
// CloseWatcher constructor both use it. Context-free, so runtime binds it once
// per realm right after installation.
struct AbortPorts {
  bool (*brand)(JS::HandleValue) = nullptr;
  bool (*aborted)(JS::HandleValue) = nullptr;
  bool (*follow)(JSContext*, JS::HandleValue, JS::HandleValue) = nullptr;
};
}  // namespace ruyijs::interaction
