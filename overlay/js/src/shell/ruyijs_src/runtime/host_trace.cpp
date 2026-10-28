#include "runtime/host_bridge.h"
#include "runtime/page_helpers.h"

namespace ruyijs {
bool HostBridge::RegisterFunction(JS::HandleObject function, JS::HandleString name) {
  return helpers_->Member(function, "Host", name, "call");
}
}
