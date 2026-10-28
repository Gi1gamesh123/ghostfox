#include "resource_network/async_http_internal.h"
#include <chrono>
#include <utility>

namespace ruyijs::resource_network {
bool AsyncHttp::Wait(uint32_t ms) {
  std::unique_lock<std::mutex> guard(inbox_->lock);
  return inbox_->ready.wait_for(guard, std::chrono::milliseconds(ms),
                                [&] { return bool(inbox_->head); });
}
void AsyncHttp::PumpQueue() noexcept {
  while (active_ < kAsyncActive && !queued_.empty()) {
    const uint64_t waiting = queued_.front();
    queued_.pop_front();
    auto found = jobs_.find(waiting);
    if (found == jobs_.end() || !found->second->source->model) continue;
    Launch(waiting);
  }
}
}
